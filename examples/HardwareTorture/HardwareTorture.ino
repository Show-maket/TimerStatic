#include <Arduino.h>
#include <TimerStatic.h>

#include <new>
#include <string.h>
#include <utility>

// The custom black-car board definition links with --wrap for the allocation
// functions. Production firmware supplies diagnostic wrappers of its own; this
// standalone test forwards them unchanged so C++ new/new[] can link. Keep this
// in the sketch, not in TimerStatic itself.
#if defined(CAR_v4_0) && !defined(TIMERSTATIC_HOST_REFERENCE)
extern "C" void* __real_malloc(size_t size);
extern "C" void* __real_calloc(size_t count, size_t size);
extern "C" void* __real_realloc(void* pointer, size_t size);
extern "C" void* __wrap_malloc(size_t size) { return __real_malloc(size); }
extern "C" void* __wrap_calloc(size_t count, size_t size) {
  return __real_calloc(count, size);
}
extern "C" void* __wrap_realloc(void* pointer, size_t size) {
  return __real_realloc(pointer, size);
}
#endif

#if defined(ARDUINO_ARCH_STM32) && !defined(TIMERSTATIC_HOST_REFERENCE)
#include <IWatchdog.h>
#define TS_SERIAL SerialUSB
#else
#define TS_SERIAL Serial
#endif

// TimerStatic hardware torture suite.
//
// Output is intentionally bounded and machine-readable:
//   TS,CASE,<name>,PASS,checks=<n>
//   TS,CASE,<name>,FAIL,checks=<n>,line=<line>,actual=<a>,expected=<e>
//   TS,SUMMARY,PASS|FAIL,...
//   TS,SUITE,STOP
//
// All synthetic time tests use injected TimeFunc functions.  The final case
// exercises the board's real millis()/micros().  No test changes library
// behaviour or reaches into Timer's private list.

namespace {

static constexpr uint32_t kCanaryA = 0x13579BDFUL;
static constexpr uint32_t kCanaryB = 0x2468ACE0UL;
static constexpr uint32_t kSuiteDeadlineMs = 20000UL;
static constexpr uint32_t kSerialWaitMs = 1500UL;
static constexpr size_t kEventCapacity = 256U;
static constexpr size_t kStressTimers = 96U;

static uint32_t gFakeMillis = 0;
static uint32_t gFakeMicros = 0;
static bool gSuiteDone = false;
static bool gSuitePassed = false;

static unsigned long fakeMillisClock() { return gFakeMillis; }
static unsigned long fakeMicrosClock() { return gFakeMicros; }

static void serviceWatchdog()
{
#if defined(ARDUINO_ARCH_STM32) && !defined(TIMERSTATIC_HOST_REFERENCE)
  if (IWatchdog.isEnabled()) IWatchdog.reload();
#endif
  yield();
}

struct EventLog {
  uint32_t before;
  uint16_t values[kEventCapacity];
  size_t size;
  bool overflow;
  uint32_t after;

  EventLog() : before(kCanaryA), size(0), overflow(false), after(kCanaryB) {}

  void reset()
  {
    size = 0;
    overflow = false;
  }

  void push(uint16_t value)
  {
    if (size < kEventCapacity) values[size++] = value;
    else overflow = true;
  }

  bool intact() const
  {
    return before == kCanaryA && after == kCanaryB && !overflow;
  }
};

static EventLog gEvents;

struct CaseResult {
  const char* name;
  bool pass;
  uint32_t checks;
  uint16_t failureLine;
  uint32_t actual;
  uint32_t expected;

  explicit CaseResult(const char* caseName)
      : name(caseName), pass(true), checks(0), failureLine(0), actual(0), expected(0) {}
};

static void expectEqual(CaseResult& result, uint32_t actual, uint32_t expected,
                        uint16_t line)
{
  ++result.checks;
  if (actual == expected || !result.pass) return;
  result.pass = false;
  result.failureLine = line;
  result.actual = actual;
  result.expected = expected;
}

static void expectTrue(CaseResult& result, bool value, uint16_t line)
{
  expectEqual(result, value ? 1U : 0U, 1U, line);
}

#define TS_EXPECT_EQ(result, actual, expected) \
  expectEqual((result), static_cast<uint32_t>(actual), \
              static_cast<uint32_t>(expected), static_cast<uint16_t>(__LINE__))
#define TS_EXPECT_TRUE(result, value) \
  expectTrue((result), static_cast<bool>(value), static_cast<uint16_t>(__LINE__))
#define TS_EXPECT_FALSE(result, value) \
  expectEqual((result), static_cast<bool>(value) ? 1U : 0U, 0U, \
              static_cast<uint16_t>(__LINE__))

static void expectEvent(CaseResult& result, size_t index, uint16_t expected)
{
  ++result.checks;
  const uint32_t actual = index < gEvents.size ? gEvents.values[index] : 0xFFFFFFFFUL;
  if (actual == expected || !result.pass) return;
  result.pass = false;
  result.failureLine = static_cast<uint16_t>(__LINE__);
  result.actual = actual;
  result.expected = expected;
}

static void finishCanaryChecks(CaseResult& result)
{
  TS_EXPECT_TRUE(result, gEvents.intact());
}

struct CallbackContext {
  uint32_t before;
  uint16_t id;
  uint16_t calls;
  Timer* self;
  Timer** victim;
  bool actionDone;
  uint32_t after;

  explicit CallbackContext(uint16_t eventId = 0)
      : before(kCanaryA), id(eventId), calls(0), self(nullptr), victim(nullptr),
        actionDone(false), after(kCanaryB) {}

  bool intact() const { return before == kCanaryA && after == kCanaryB; }
};

struct GuardedTimer {
  uint32_t before;
  Timer timer;
  uint32_t after;

  GuardedTimer() : before(kCanaryA), timer(), after(kCanaryB) {}
  bool intact() const { return before == kCanaryA && after == kCanaryB; }
};

static void recordCallback(void* raw)
{
  CallbackContext* context = static_cast<CallbackContext*>(raw);
  ++context->calls;
  gEvents.push(context->id);
}

static void rearmDelayCallback(void* raw)
{
  CallbackContext* context = static_cast<CallbackContext*>(raw);
  recordCallback(raw);
  if (!context->actionDone) {
    context->actionDone = true;
    context->self->delay(5, fakeMillisClock, rearmDelayCallback);
  }
}

static void resetToEndCallback(void* raw)
{
  CallbackContext* context = static_cast<CallbackContext*>(raw);
  recordCallback(raw);
  if (!context->actionDone) {
    context->actionDone = true;
    context->self->resetToEnd();
  }
}

static void deleteVictimCallback(void* raw)
{
  CallbackContext* context = static_cast<CallbackContext*>(raw);
  recordCallback(raw);
  if (context->victim != nullptr && *context->victim != nullptr) {
    Timer* victim = *context->victim;
    *context->victim = nullptr;
    delete victim;
  }
}

static void recursiveTickCallback(void* raw)
{
  recordCallback(raw);
  Timer::tick();
}

struct CreateContext {
  uint32_t before;
  CallbackContext parent;
  CallbackContext children[2];
  Timer* created[2];
  uint32_t after;

  CreateContext()
      : before(kCanaryA), parent(1), children{CallbackContext(10), CallbackContext(11)},
        created{nullptr, nullptr}, after(kCanaryB) {}

  bool intact() const
  {
    return before == kCanaryA && after == kCanaryB && parent.intact() &&
           children[0].intact() && children[1].intact();
  }
};

static void createDuringCallback(void* raw)
{
  CreateContext* context = static_cast<CreateContext*>(raw);
  ++context->parent.calls;
  gEvents.push(context->parent.id);
  for (size_t i = 0; i < 2; ++i) {
    context->created[i] = new (std::nothrow) Timer(&context->children[i]);
    if (context->created[i] != nullptr) {
      context->created[i]->delay(0, fakeMillisClock, recordCallback);
    }
  }
}

struct FiniteContext {
  uint32_t before;
  uint16_t id;
  uint16_t calls;
  uint16_t lastCalls;
  Timer* self;
  uint32_t after;

  explicit FiniteContext(uint16_t eventId)
      : before(kCanaryA), id(eventId), calls(0), lastCalls(0), self(nullptr),
        after(kCanaryB) {}

  bool intact() const { return before == kCanaryA && after == kCanaryB; }
};

static void finiteCallback(void* raw)
{
  FiniteContext* context = static_cast<FiniteContext*>(raw);
  ++context->calls;
  if (context->self->isForLast()) ++context->lastCalls;
  gEvents.push(context->id);
}

struct MoveActiveContext {
  uint32_t before;
  uint16_t calls;
  bool moved;
  Timer* source;
  Timer* destination;
  uint32_t after;

  MoveActiveContext()
      : before(kCanaryA), calls(0), moved(false), source(nullptr), destination(nullptr),
        after(kCanaryB) {}

  bool intact() const { return before == kCanaryA && after == kCanaryB; }
};

static void moveActiveCallback(void* raw)
{
  MoveActiveContext* context = static_cast<MoveActiveContext*>(raw);
  ++context->calls;
  gEvents.push(static_cast<uint16_t>(30U + context->calls));
  if (!context->moved) {
    context->moved = true;
    *context->destination = std::move(*context->source);
  }
}

static CaseResult testZeroPeriod()
{
  CaseResult result("zero_period_policy");
  gEvents.reset();
  gFakeMillis = 0;

  CallbackContext zeroDelay(1);
  {
    Timer timer(&zeroDelay);
    timer.delay(0, fakeMillisClock, recordCallback);
    TS_EXPECT_TRUE(result, timer.isRun());
    Timer::tick();
    Timer::tick();
    TS_EXPECT_EQ(result, zeroDelay.calls, 1);
    TS_EXPECT_FALSE(result, timer.isRun());
  }

  CallbackContext zeroPeriodic(2);
  {
    Timer timer(&zeroPeriodic);
    timer.set(0, fakeMillisClock, recordCallback, false);
    TS_EXPECT_FALSE(result, timer.isRun());
    timer.ON();
    TS_EXPECT_FALSE(result, timer.isRun());
    timer.forCount(0, fakeMillisClock, recordCallback, 3, false);
    TS_EXPECT_FALSE(result, timer.isRun());
    timer.forTime(0, fakeMillisClock, recordCallback, 10, false);
    TS_EXPECT_FALSE(result, timer.isRun());
    Timer::tick();
    TS_EXPECT_EQ(result, zeroPeriodic.calls, 0);
  }

  TS_EXPECT_TRUE(result, zeroDelay.intact());
  TS_EXPECT_TRUE(result, zeroPeriodic.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testSelfRearm()
{
  CaseResult result("self_rearm");
  gEvents.reset();
  gFakeMillis = 0;

  CallbackContext rearm(1);
  {
    Timer timer(&rearm);
    rearm.self = &timer;
    timer.delay(5, fakeMillisClock, rearmDelayCallback);
    gFakeMillis = 5;
    Timer::tick();
    Timer::tick();
    TS_EXPECT_EQ(result, rearm.calls, 1);
    TS_EXPECT_TRUE(result, timer.isRun());
    gFakeMillis = 10;
    Timer::tick();
    TS_EXPECT_EQ(result, rearm.calls, 2);
    TS_EXPECT_FALSE(result, timer.isRun());
  }

  CallbackContext resetEnd(2);
  {
    Timer timer(&resetEnd);
    resetEnd.self = &timer;
    gFakeMillis = 0;
    timer.set(10, fakeMillisClock, resetToEndCallback, false);
    gFakeMillis = 10;
    Timer::tick();
    Timer::tick();
    Timer::tick();
    TS_EXPECT_EQ(result, resetEnd.calls, 2);
  }

#ifndef __AVR__
  uint32_t stdCanaryA = kCanaryA;
  uint32_t stdCanaryB = kCanaryB;
  uint16_t stdCalls = 0;
  {
    Timer timer;
    gFakeMillis = 0;
    timer.delay_std(5, fakeMillisClock, [&timer, &stdCalls]() {
      ++stdCalls;
      if (stdCalls == 1) {
        timer.delay_std(5, fakeMillisClock, [&stdCalls]() { ++stdCalls; });
      }
    });
    gFakeMillis = 5;
    Timer::tick();
    gFakeMillis = 10;
    Timer::tick();
    TS_EXPECT_EQ(result, stdCalls, 2);
    TS_EXPECT_FALSE(result, timer.isRun());
  }
  TS_EXPECT_EQ(result, stdCanaryA, kCanaryA);
  TS_EXPECT_EQ(result, stdCanaryB, kCanaryB);
#endif

  TS_EXPECT_TRUE(result, rearm.intact());
  TS_EXPECT_TRUE(result, resetEnd.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testSelfDelete()
{
  CaseResult result("self_delete");
  gEvents.reset();
  gFakeMillis = 0;

#ifndef __AVR__
  uint32_t before = kCanaryA;
  uint32_t after = kCanaryB;
  uint16_t selfCalls = 0;
  Timer* self = new (std::nothrow) Timer();
  CallbackContext follower(2);
  Timer followerTimer(&follower);
  if (self != nullptr) {
    self->delay_std(0, fakeMillisClock, [&self, &selfCalls]() {
      ++selfCalls;
      gEvents.push(1);
      Timer* doomed = self;
      self = nullptr;
      delete doomed;
    });
  }
  followerTimer.delay(0, fakeMillisClock, recordCallback);
  TS_EXPECT_TRUE(result, self != nullptr);
  Timer::tick();
  TS_EXPECT_EQ(result, selfCalls, 1);
  TS_EXPECT_TRUE(result, self == nullptr);
  TS_EXPECT_EQ(result, follower.calls, 1);
  TS_EXPECT_EQ(result, gEvents.size, 2);
  expectEvent(result, 0, 1);
  expectEvent(result, 1, 2);
  TS_EXPECT_EQ(result, before, kCanaryA);
  TS_EXPECT_EQ(result, after, kCanaryB);
  TS_EXPECT_TRUE(result, follower.intact());
#else
  CallbackContext selfContext(1);
  CallbackContext follower(2);
  Timer* self = new (std::nothrow) Timer(&selfContext);
  Timer followerTimer(&follower);
  selfContext.victim = &self;
  if (self != nullptr) self->delay(0, fakeMillisClock, deleteVictimCallback);
  followerTimer.delay(0, fakeMillisClock, recordCallback);
  Timer::tick();
  TS_EXPECT_TRUE(result, self == nullptr);
  TS_EXPECT_EQ(result, selfContext.calls, 1);
  TS_EXPECT_EQ(result, follower.calls, 1);
#endif

  finishCanaryChecks(result);
  return result;
}

static CaseResult testDeleteNext()
{
  CaseResult result("delete_next");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext first(1), middle(2), tail(3);
  Timer firstTimer(&first);
  Timer* middleTimer = new (std::nothrow) Timer(&middle);
  Timer tailTimer(&tail);
  first.victim = &middleTimer;
  firstTimer.delay(0, fakeMillisClock, deleteVictimCallback);
  if (middleTimer != nullptr) middleTimer->delay(0, fakeMillisClock, recordCallback);
  tailTimer.delay(0, fakeMillisClock, recordCallback);
  TS_EXPECT_TRUE(result, middleTimer != nullptr);
  Timer::tick();
  TS_EXPECT_TRUE(result, middleTimer == nullptr);
  TS_EXPECT_EQ(result, first.calls, 1);
  TS_EXPECT_EQ(result, middle.calls, 0);
  TS_EXPECT_EQ(result, tail.calls, 1);
  TS_EXPECT_EQ(result, gEvents.size, 2);
  expectEvent(result, 0, 1);
  expectEvent(result, 1, 3);
  if (middleTimer != nullptr) delete middleTimer;
  TS_EXPECT_TRUE(result, first.intact() && middle.intact() && tail.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testDeleteHead()
{
  CaseResult result("delete_head");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext head(1), middle(2), tail(3);
  Timer* headTimer = new (std::nothrow) Timer(&head);
  Timer middleTimer(&middle);
  Timer tailTimer(&tail);
  if (headTimer != nullptr) headTimer->set(100, fakeMillisClock, recordCallback, false);
  middle.victim = &headTimer;
  middleTimer.delay(0, fakeMillisClock, deleteVictimCallback);
  tailTimer.delay(0, fakeMillisClock, recordCallback);
  TS_EXPECT_TRUE(result, headTimer != nullptr);
  Timer::tick();
  TS_EXPECT_TRUE(result, headTimer == nullptr);
  TS_EXPECT_EQ(result, head.calls, 0);
  TS_EXPECT_EQ(result, middle.calls, 1);
  TS_EXPECT_EQ(result, tail.calls, 1);
  TS_EXPECT_EQ(result, gEvents.size, 2);
  expectEvent(result, 0, 2);
  expectEvent(result, 1, 3);
  if (headTimer != nullptr) delete headTimer;
  TS_EXPECT_TRUE(result, head.intact() && middle.intact() && tail.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testDeleteTail()
{
  CaseResult result("delete_tail");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext head(1), middle(2), tail(3);
  Timer headTimer(&head);
  Timer middleTimer(&middle);
  Timer* tailTimer = new (std::nothrow) Timer(&tail);
  head.victim = &tailTimer;
  headTimer.delay(0, fakeMillisClock, deleteVictimCallback);
  middleTimer.delay(0, fakeMillisClock, recordCallback);
  if (tailTimer != nullptr) tailTimer->delay(0, fakeMillisClock, recordCallback);
  TS_EXPECT_TRUE(result, tailTimer != nullptr);
  Timer::tick();
  TS_EXPECT_TRUE(result, tailTimer == nullptr);
  TS_EXPECT_EQ(result, head.calls, 1);
  TS_EXPECT_EQ(result, middle.calls, 1);
  TS_EXPECT_EQ(result, tail.calls, 0);
  TS_EXPECT_EQ(result, gEvents.size, 2);
  expectEvent(result, 0, 1);
  expectEvent(result, 1, 2);
  if (tailTimer != nullptr) delete tailTimer;
  TS_EXPECT_TRUE(result, head.intact() && middle.intact() && tail.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testCreateDuringCallback()
{
  CaseResult result("create_during_callback");
  gEvents.reset();
  gFakeMillis = 0;
  CreateContext create;
  CallbackContext follower(2);
  Timer creator(&create);
  Timer followerTimer(&follower);
  creator.delay(0, fakeMillisClock, createDuringCallback);
  followerTimer.delay(0, fakeMillisClock, recordCallback);
  Timer::tick();
  TS_EXPECT_EQ(result, create.parent.calls, 1);
  TS_EXPECT_EQ(result, follower.calls, 1);
  TS_EXPECT_EQ(result, create.children[0].calls, 0);
  TS_EXPECT_EQ(result, create.children[1].calls, 0);
  TS_EXPECT_EQ(result, gEvents.size, 2);
  expectEvent(result, 0, 1);
  expectEvent(result, 1, 2);
  TS_EXPECT_TRUE(result, create.created[0] != nullptr && create.created[1] != nullptr);
  Timer::tick();
  TS_EXPECT_EQ(result, create.children[0].calls, 1);
  TS_EXPECT_EQ(result, create.children[1].calls, 1);
  TS_EXPECT_EQ(result, gEvents.size, 4);
  expectEvent(result, 2, 10);
  expectEvent(result, 3, 11);
  for (size_t i = 0; i < 2; ++i) {
    delete create.created[i];
    create.created[i] = nullptr;
  }
  TS_EXPECT_TRUE(result, create.intact() && follower.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testMoveConstructor()
{
  CaseResult result("move_constructor");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext context(7);
  {
    Timer source(&context);
    source.delay(5, fakeMillisClock, recordCallback);
    Timer moved(std::move(source));
    TS_EXPECT_FALSE(result, source.isRun());
    TS_EXPECT_TRUE(result, moved.isRun());
    gFakeMillis = 5;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 1);
    TS_EXPECT_FALSE(result, moved.isRun());
  }
  TS_EXPECT_EQ(result, gEvents.size, 1);
  expectEvent(result, 0, 7);
  TS_EXPECT_TRUE(result, context.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testMoveAssignment()
{
  CaseResult result("move_assignment");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext oldContext(1), sourceContext(2);
  {
    Timer destination(&oldContext);
    Timer source(&sourceContext);
    destination.delay(5, fakeMillisClock, recordCallback);
    source.delay(5, fakeMillisClock, recordCallback);
    destination = std::move(source);
    TS_EXPECT_FALSE(result, source.isRun());
    TS_EXPECT_TRUE(result, destination.isRun());
    gFakeMillis = 5;
    Timer::tick();
    TS_EXPECT_EQ(result, oldContext.calls, 0);
    TS_EXPECT_EQ(result, sourceContext.calls, 1);
  }
  TS_EXPECT_EQ(result, gEvents.size, 1);
  expectEvent(result, 0, 2);
  TS_EXPECT_TRUE(result, oldContext.intact() && sourceContext.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testMoveActive()
{
  CaseResult result("move_active_callback");
  gEvents.reset();
  gFakeMillis = 0;
  MoveActiveContext context;
  {
    Timer destination;
    Timer source(&context);
    context.destination = &destination;
    context.source = &source;
    source.set(5, fakeMillisClock, moveActiveCallback, false);
    gFakeMillis = 5;
    Timer::tick();
    TS_EXPECT_TRUE(result, context.moved);
    TS_EXPECT_EQ(result, context.calls, 1);
    TS_EXPECT_FALSE(result, source.isRun());
    TS_EXPECT_TRUE(result, destination.isRun());
    gFakeMillis = 10;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 2);
    destination.OFF();
  }
  TS_EXPECT_EQ(result, gEvents.size, 2);
  expectEvent(result, 0, 31);
  expectEvent(result, 1, 32);
  TS_EXPECT_TRUE(result, context.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testRecursiveTick()
{
  CaseResult result("recursive_tick_guard");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext first(1), second(2);
  {
    Timer firstTimer(&first), secondTimer(&second);
    firstTimer.delay(0, fakeMillisClock, recursiveTickCallback);
    secondTimer.delay(0, fakeMillisClock, recordCallback);
    Timer::tick();
    TS_EXPECT_EQ(result, first.calls, 1);
    TS_EXPECT_EQ(result, second.calls, 1);
    Timer::tick();
    TS_EXPECT_EQ(result, first.calls, 1);
    TS_EXPECT_EQ(result, second.calls, 1);
  }
  TS_EXPECT_EQ(result, gEvents.size, 2);
  expectEvent(result, 0, 1);
  expectEvent(result, 1, 2);
  TS_EXPECT_TRUE(result, first.intact() && second.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testControls()
{
  CaseResult result("off_on_restart_setperiod");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext context(1);
  {
    Timer timer(&context);
    timer.set(10, fakeMillisClock, recordCallback, false);
    timer.OFF();
    gFakeMillis = 20;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 0);
    timer.ON();
    TS_EXPECT_TRUE(result, timer.isRun());
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 1);

    timer.OFF();
    gFakeMillis = 25;
    TS_EXPECT_TRUE(result, timer.restart());
    gFakeMillis = 34;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 1);
    gFakeMillis = 35;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 2);

    timer.setPeriod(4);
    gFakeMillis = 44;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 2);
    gFakeMillis = 45;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 3);
    timer.restartWithPeriod(7);
    TS_EXPECT_EQ(result, timer.getPeriod(), 7);
    gFakeMillis = 51;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 3);
    gFakeMillis = 52;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 4);
  }
  TS_EXPECT_TRUE(result, context.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testLateCatchup()
{
  CaseResult result("late_catchup_single_fire");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext periodic(1);
  {
    Timer timer(&periodic);
    timer.set(10, fakeMillisClock, recordCallback, false);
    gFakeMillis = 35;
    Timer::tick();
    TS_EXPECT_EQ(result, periodic.calls, 1);
    TS_EXPECT_EQ(result, timer.getRemainingTime(), 5);
    Timer::tick();
    TS_EXPECT_EQ(result, periodic.calls, 1);
    gFakeMillis = 40;
    Timer::tick();
    TS_EXPECT_EQ(result, periodic.calls, 2);
    TS_EXPECT_EQ(result, timer.getRemainingTime(), 10);
  }

  gFakeMillis = 0;
  CallbackContext counted(2);
  {
    Timer timer(&counted);
    timer.forCount(10, fakeMillisClock, recordCallback, 3, false);
    gFakeMillis = 35;
    Timer::tick();
    TS_EXPECT_EQ(result, counted.calls, 1);
    gFakeMillis = 40;
    Timer::tick();
    gFakeMillis = 50;
    Timer::tick();
    TS_EXPECT_EQ(result, counted.calls, 3);
    TS_EXPECT_FALSE(result, timer.isRun());
  }
  TS_EXPECT_TRUE(result, periodic.intact() && counted.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testForCount()
{
  CaseResult result("for_count_last_and_extend");
  gEvents.reset();
  gFakeMillis = 0;
  FiniteContext context(1);
  {
    Timer timer(&context);
    context.self = &timer;
    timer.forCount(10, fakeMillisClock, finiteCallback, 3, false);
    for (uint32_t time = 10; time <= 30; time += 10) {
      gFakeMillis = time;
      Timer::tick();
    }
    TS_EXPECT_EQ(result, context.calls, 3);
    TS_EXPECT_EQ(result, context.lastCalls, 1);
    TS_EXPECT_FALSE(result, timer.isRun());
    TS_EXPECT_FALSE(result, timer.isForLast());
  }

  struct ExtendContext {
    uint32_t before;
    Timer* timer;
    uint16_t calls;
    uint16_t lastCalls;
    bool extended;
    uint32_t after;
  } extend{kCanaryA, nullptr, 0, 0, false, kCanaryB};
  auto extendCallback = [](void* raw) {
    ExtendContext* context = static_cast<ExtendContext*>(raw);
    ++context->calls;
    if (context->timer->isForLast()) {
      ++context->lastCalls;
      if (!context->extended) {
        context->extended = true;
        context->timer->setLifeCount(2);
      }
    }
  };
  gFakeMillis = 0;
  {
    Timer timer(&extend);
    extend.timer = &timer;
    timer.forCount(5, fakeMillisClock, extendCallback, 1, false);
    gFakeMillis = 5;
    Timer::tick();
    gFakeMillis = 10;
    Timer::tick();
    gFakeMillis = 15;
    Timer::tick();
    TS_EXPECT_EQ(result, extend.calls, 3);
    TS_EXPECT_EQ(result, extend.lastCalls, 2);
    TS_EXPECT_FALSE(result, timer.isRun());
  }
  TS_EXPECT_EQ(result, extend.before, kCanaryA);
  TS_EXPECT_EQ(result, extend.after, kCanaryB);
  TS_EXPECT_TRUE(result, context.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testForTime()
{
  CaseResult result("for_time_regular_and_late");
  gEvents.reset();
  gFakeMillis = 0;
  FiniteContext regular(1);
  {
    Timer timer(&regular);
    regular.self = &timer;
    timer.forTime(10, fakeMillisClock, finiteCallback, 25, false);
    for (uint32_t time = 10; time <= 30; time += 10) {
      gFakeMillis = time;
      Timer::tick();
    }
    TS_EXPECT_EQ(result, regular.calls, 3);
    TS_EXPECT_EQ(result, regular.lastCalls, 1);
    TS_EXPECT_FALSE(result, timer.isRun());
  }

  gFakeMillis = 0;
  FiniteContext late(2);
  {
    Timer timer(&late);
    late.self = &timer;
    timer.forTime(10, fakeMillisClock, finiteCallback, 35, false);
    gFakeMillis = 45;
    Timer::tick();
    TS_EXPECT_EQ(result, late.calls, 1);
    TS_EXPECT_EQ(result, late.lastCalls, 1);
    TS_EXPECT_FALSE(result, timer.isRun());
  }
  TS_EXPECT_TRUE(result, regular.intact() && late.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testMillisWrap()
{
  CaseResult result("millis_wrap_injected");
  gEvents.reset();
  gFakeMillis = 0xFFFFFFF0UL;
  CallbackContext context(1);
  {
    Timer timer(&context);
    timer.set(20, fakeMillisClock, recordCallback, false);
    TS_EXPECT_EQ(result, timer.getRemainingTime(), 20);
    gFakeMillis = 0xFFFFFFFFUL;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 0);
    TS_EXPECT_EQ(result, timer.getRemainingTime(), 5);
    gFakeMillis = 4;
    TS_EXPECT_TRUE(result, timer.isTimeExpired());
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 1);
    TS_EXPECT_EQ(result, timer.getRemainingTime(), 20);
  }
  TS_EXPECT_TRUE(result, context.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testMicrosWrap()
{
  CaseResult result("micros_wrap_and_catchup");
  gEvents.reset();
  gFakeMicros = 0xFFFFFFF0UL;
  CallbackContext context(1);
  {
    Timer timer(&context);
    timer.set(8, fakeMicrosClock, recordCallback, false);
    gFakeMicros = 0x00000010UL;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 1);
    TS_EXPECT_EQ(result, timer.getRemainingTime(), 8);
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 1);
    gFakeMicros = 0x00000018UL;
    Timer::tick();
    TS_EXPECT_EQ(result, context.calls, 2);
  }
  TS_EXPECT_TRUE(result, context.intact());
  finishCanaryChecks(result);
  return result;
}

static CaseResult testStress96()
{
  CaseResult result("stress_96_order_count_canary");
  gEvents.reset();
  gFakeMillis = 0;
  CallbackContext* contexts = new (std::nothrow) CallbackContext[kStressTimers];
  GuardedTimer* timers = new (std::nothrow) GuardedTimer[kStressTimers];
  TS_EXPECT_TRUE(result, contexts != nullptr);
  TS_EXPECT_TRUE(result, timers != nullptr);
  if (contexts != nullptr && timers != nullptr) {
    for (size_t i = 0; i < kStressTimers; ++i) {
      contexts[i].id = static_cast<uint16_t>(i);
      timers[i].timer.setObj(&contexts[i]);
      timers[i].timer.forCount(1, fakeMillisClock, recordCallback, 2, false);
    }
    gFakeMillis = 1;
    Timer::tick();
    gFakeMillis = 2;
    Timer::tick();
    TS_EXPECT_EQ(result, gEvents.size, kStressTimers * 2U);
    for (size_t pass = 0; pass < 2; ++pass) {
      for (size_t i = 0; i < kStressTimers; ++i) {
        expectEvent(result, pass * kStressTimers + i, static_cast<uint16_t>(i));
        TS_EXPECT_EQ(result, contexts[i].calls, 2);
        TS_EXPECT_TRUE(result, contexts[i].intact());
        TS_EXPECT_TRUE(result, timers[i].intact());
      }
    }
    delete[] timers;
    timers = nullptr;
    const size_t eventsBeforeEmptyTick = gEvents.size;
    gFakeMillis = 3;
    Timer::tick();
    TS_EXPECT_EQ(result, gEvents.size, eventsBeforeEmptyTick);
  }
  delete[] timers;
  delete[] contexts;
  finishCanaryChecks(result);
  return result;
}

static CaseResult testRealClocks()
{
  CaseResult result("real_millis_micros_smoke");
  gEvents.reset();
  CallbackContext millisContext(1), microsContext(2);
  {
    Timer millisTimer(&millisContext), microsTimer(&microsContext);
    millisTimer.delay(5, millis, recordCallback);
    microsTimer.delay(2000, micros, recordCallback);
    const uint32_t start = millis();
    uint32_t iterations = 0;
    while ((millisContext.calls == 0 || microsContext.calls == 0) &&
           static_cast<uint32_t>(millis() - start) < 250UL && iterations < 500000UL) {
      Timer::tick();
      serviceWatchdog();
      ++iterations;
    }
    TS_EXPECT_EQ(result, millisContext.calls, 1);
    TS_EXPECT_EQ(result, microsContext.calls, 1);
    TS_EXPECT_TRUE(result, iterations < 500000UL);
  }
  TS_EXPECT_TRUE(result, millisContext.intact() && microsContext.intact());
  finishCanaryChecks(result);
  return result;
}

typedef CaseResult (*CaseFunction)();

struct CaseDefinition {
  const char* name;
  CaseFunction run;
};

static const CaseDefinition kCases[] = {
    {"zero_period_policy", testZeroPeriod},
    {"self_rearm", testSelfRearm},
    {"self_delete", testSelfDelete},
    {"delete_next", testDeleteNext},
    {"delete_head", testDeleteHead},
    {"delete_tail", testDeleteTail},
    {"create_during_callback", testCreateDuringCallback},
    {"move_constructor", testMoveConstructor},
    {"move_assignment", testMoveAssignment},
    {"move_active_callback", testMoveActive},
    {"recursive_tick_guard", testRecursiveTick},
    {"off_on_restart_setperiod", testControls},
    {"late_catchup_single_fire", testLateCatchup},
    {"for_count_last_and_extend", testForCount},
    {"for_time_regular_and_late", testForTime},
    {"millis_wrap_injected", testMillisWrap},
    {"micros_wrap_and_catchup", testMicrosWrap},
    {"stress_96_order_count_canary", testStress96},
    {"real_millis_micros_smoke", testRealClocks},
};

static void emitCase(const CaseResult& result)
{
  TS_SERIAL.print(F("TS,CASE,"));
  TS_SERIAL.print(result.name);
  TS_SERIAL.print(result.pass ? F(",PASS,checks=") : F(",FAIL,checks="));
  TS_SERIAL.print(result.checks);
  if (!result.pass) {
    TS_SERIAL.print(F(",line="));
    TS_SERIAL.print(result.failureLine);
    TS_SERIAL.print(F(",actual="));
    TS_SERIAL.print(result.actual);
    TS_SERIAL.print(F(",expected="));
    TS_SERIAL.print(result.expected);
  }
  TS_SERIAL.println();
}

}  // namespace

void setup()
{
#if defined(ARDUINO_ARCH_STM32) && !defined(TIMERSTATIC_HOST_REFERENCE)
  if (!IWatchdog.isEnabled()) IWatchdog.begin(8000000UL);
#endif

  TS_SERIAL.begin(115200);
  const uint32_t serialWaitStart = millis();
  while (!TS_SERIAL && static_cast<uint32_t>(millis() - serialWaitStart) < kSerialWaitMs) {
    serviceWatchdog();
  }

  const size_t caseCount = sizeof(kCases) / sizeof(kCases[0]);
  TS_SERIAL.print(F("TS,SUITE,START,version=1,cases="));
  TS_SERIAL.print(caseCount);
  TS_SERIAL.print(F(",stress_timers="));
  TS_SERIAL.print(kStressTimers);
  TS_SERIAL.print(F(",deadline_ms="));
  TS_SERIAL.println(kSuiteDeadlineMs);

  const uint32_t suiteStart = millis();
  uint32_t totalChecks = 0;
  uint16_t passed = 0;
  uint16_t failed = 0;

  for (size_t i = 0; i < caseCount; ++i) {
    serviceWatchdog();
    if (static_cast<uint32_t>(millis() - suiteStart) >= kSuiteDeadlineMs) {
      CaseResult deadline("suite_deadline");
      deadline.pass = false;
      deadline.checks = 1;
      deadline.failureLine = static_cast<uint16_t>(__LINE__);
      deadline.actual = static_cast<uint32_t>(millis() - suiteStart);
      deadline.expected = kSuiteDeadlineMs - 1U;
      emitCase(deadline);
      ++failed;
      break;
    }
    CaseResult result = kCases[i].run();
    if (strcmp(result.name, kCases[i].name) != 0) {
      result.pass = false;
      result.failureLine = static_cast<uint16_t>(__LINE__);
    }
    emitCase(result);
    totalChecks += result.checks;
    if (result.pass) ++passed;
    else ++failed;
  }

  gSuitePassed = failed == 0 && passed == caseCount;
  TS_SERIAL.print(gSuitePassed ? F("TS,SUMMARY,PASS,passed=") : F("TS,SUMMARY,FAIL,passed="));
  TS_SERIAL.print(passed);
  TS_SERIAL.print(F(",failed="));
  TS_SERIAL.print(failed);
  TS_SERIAL.print(F(",checks="));
  TS_SERIAL.print(totalChecks);
  TS_SERIAL.print(F(",elapsed_ms="));
  TS_SERIAL.println(static_cast<uint32_t>(millis() - suiteStart));
  TS_SERIAL.println(F("TS,SUITE,STOP"));
  TS_SERIAL.flush();
  gSuiteDone = true;
}

void loop()
{
  // Deliberately do not call Timer::tick() after the terminal record.
  // Keep an already-enabled watchdog alive without producing more output.
  serviceWatchdog();
  delay(1);
}

#ifdef TIMERSTATIC_HOST_REFERENCE
int timerStaticTortureExitCode()
{
  return gSuiteDone && gSuitePassed ? 0 : 1;
}
#endif
