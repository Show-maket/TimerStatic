#include "TimerStatic.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(!std::is_copy_constructible<Timer>::value,
              "Timer addresses must remain stable");
static_assert(!std::is_copy_assignable<Timer>::value,
              "Timer addresses must remain stable");
static_assert(std::is_nothrow_move_constructible<Timer>::value,
              "Timer must remain container-friendly");
static_assert(std::is_nothrow_move_assignable<Timer>::value,
              "Timer move assignment must remain container-friendly");

namespace {

uint32_t nowMs = 0;
int plainCalls = 0;
int paramCalls = 0;
int stdCalls = 0;
int childCalls = 0;
Timer* createdTimer = nullptr;
Timer* selfRearmTimer = nullptr;

Timer* selfDeleteTimer = nullptr;
Timer* stdSelfDeleteTimer = nullptr;
Timer* nextVictim = nullptr;
Timer* oldLastVictim = nullptr;
#if TIMER_STATIC_ENABLE_OBSERVER
Timer* observerVictim = nullptr;
#endif
int mutationFirstCalls = 0;
int mutationVictimCalls = 0;
int mutationTailCalls = 0;

unsigned long mockMillis() { return nowMs; }
void plainCallback() { ++plainCalls; }
void childCallback() { ++childCalls; }
void victimCallback() { ++mutationVictimCalls; }
void tailCallback() { ++mutationTailCalls; }

void paramCallback(void* value) {
    assert(value == reinterpret_cast<void*>(0x1234));
    ++paramCalls;
}

void selfRearmCallback() {
    ++plainCalls;
    if (plainCalls == 1) {
        selfRearmTimer->delay(7, mockMillis, selfRearmCallback);
    }
}

struct ParamRearmState {
    Timer* timer = nullptr;
    int calls = 0;
};

void paramSelfRearmCallback(void* rawState) {
    auto* state = static_cast<ParamRearmState*>(rawState);
    ++state->calls;
    if (state->calls == 1) {
        state->timer->delay(4, mockMillis, paramSelfRearmCallback);
    }
}

void deleteSelfCallback() {
    ++mutationFirstCalls;
    Timer* doomed = selfDeleteTimer;
    selfDeleteTimer = nullptr;
    delete doomed;
}

void deleteNextCallback() {
    ++mutationFirstCalls;
    Timer* doomed = nextVictim;
    nextVictim = nullptr;
    delete doomed;
}

void deleteOldLastAndCreateCallback() {
    ++mutationFirstCalls;
    Timer* doomed = oldLastVictim;
    oldLastVictim = nullptr;
    delete doomed;

    createdTimer = new Timer();
    createdTimer->delay(0, mockMillis, childCallback);
}

#if TIMER_STATIC_ENABLE_OBSERVER
void deletingObserver(const Timer* timer) {
    if (timer != observerVictim) return;
    Timer* doomed = observerVictim;
    observerVictim = nullptr;
    delete doomed;
}
#endif

void resetCounters() {
    nowMs = 0;
    plainCalls = 0;
    paramCalls = 0;
    stdCalls = 0;
    childCalls = 0;
    mutationFirstCalls = 0;
    mutationVictimCalls = 0;
    mutationTailCalls = 0;
}

void testCallbackTransitions() {
    {
        resetCounters();
        Timer timer(reinterpret_cast<void*>(0x1234));
        timer.delay_std(1, mockMillis, [] { ++stdCalls; });
        timer.delay(1, mockMillis, plainCallback);
        nowMs = 1;
        timer.check();
        assert(plainCalls == 1 && paramCalls == 0 && stdCalls == 0);
    }
    {
        resetCounters();
        Timer timer(reinterpret_cast<void*>(0x1234));
        timer.delay_std(1, mockMillis, [] { ++stdCalls; });
        timer.delay(1, mockMillis, paramCallback);
        nowMs = 1;
        timer.check();
        assert(plainCalls == 0 && paramCalls == 1 && stdCalls == 0);
    }
    {
        resetCounters();
        Timer timer(reinterpret_cast<void*>(0x1234));
        timer.delay(1, mockMillis, plainCallback);
        timer.delay_std(1, mockMillis, [] { ++stdCalls; });
        nowMs = 1;
        timer.check();
        assert(plainCalls == 0 && paramCalls == 0 && stdCalls == 1);
    }
    {
        resetCounters();
        Timer timer(reinterpret_cast<void*>(0x1234));
        timer.delay(1, mockMillis, paramCallback);
        timer.delay(1, mockMillis, plainCallback);
        nowMs = 1;
        timer.check();
        assert(plainCalls == 1 && paramCalls == 0 && stdCalls == 0);
    }
    {
        resetCounters();
        Timer timer(reinterpret_cast<void*>(0x1234));
        timer.set_std(1, mockMillis, [] { ++stdCalls; });
        timer.setCallback(paramCallback);
        nowMs = 1;
        timer.check();
        assert(plainCalls == 0 && paramCalls == 1 && stdCalls == 0);
    }
}

void testConstantTimeCatchUp() {
    resetCounters();
    Timer timer(1, mockMillis, std::function<void()>([] { ++stdCalls; }));
    nowMs = 0x40000000u;
    timer.check();
    assert(stdCalls == 1);
    timer.check();
    assert(stdCalls == 1);
}

void testConstructorFirstFireCompatibility() {
    resetCounters();
    Timer timer(100, mockMillis,
                std::function<void()>([] { ++stdCalls; }));
    Timer::tick();
    assert(stdCalls == 1);
}

void testReentrantTickIsIgnored() {
    resetCounters();
    Timer timer(1, mockMillis, std::function<void()>([] {
        ++stdCalls;
        Timer::tick();
    }));
    Timer::tick();
    assert(stdCalls == 1);
}

void testNewTimerWaitsForNextPass() {
    resetCounters();
    Timer parent;
    parent.delay_std(0, mockMillis, [] {
        ++stdCalls;
        createdTimer = new Timer();
        createdTimer->delay(0, mockMillis, childCallback);
    });

    Timer::tick();
    assert(stdCalls == 1 && childCalls == 0 && createdTimer != nullptr);
    Timer::tick();
    assert(childCalls == 1);
    delete createdTimer;
    createdTimer = nullptr;
}

void testDeleteCurrentFromPlainAndStdCallbacks() {
    resetCounters();
    selfDeleteTimer = new Timer();
    selfDeleteTimer->delay(0, mockMillis, deleteSelfCallback);
    Timer::tick();
    assert(mutationFirstCalls == 1 && selfDeleteTimer == nullptr);

    stdSelfDeleteTimer = new Timer();
    stdSelfDeleteTimer->delay_std(0, mockMillis, [] {
        ++stdCalls;
        Timer* doomed = stdSelfDeleteTimer;
        stdSelfDeleteTimer = nullptr;
        delete doomed;
    });
    Timer::tick();
    assert(stdCalls == 1 && stdSelfDeleteTimer == nullptr);

    Timer::tick();
    assert(mutationFirstCalls == 1 && stdCalls == 1);
}

void testDeleteImmediateNextFromCallback() {
    resetCounters();
    Timer* first = new Timer();
    Timer* victim = new Timer();
    Timer* tail = new Timer();
    nextVictim = victim;
    first->delay(0, mockMillis, deleteNextCallback);
    victim->delay(0, mockMillis, victimCallback);
    tail->delay(0, mockMillis, tailCallback);

    Timer::tick();
    assert(mutationFirstCalls == 1);
    assert(mutationVictimCalls == 0);
    assert(mutationTailCalls == 1);
    assert(nextVictim == nullptr);

    delete first;
    delete tail;
}

void testDeleteOldLastAndDeferReplacement() {
    resetCounters();
    Timer* first = new Timer();
    Timer* middle = new Timer();
    Timer* oldLast = new Timer();
    oldLastVictim = oldLast;
    first->delay(0, mockMillis, deleteOldLastAndCreateCallback);
    middle->delay(0, mockMillis, tailCallback);
    oldLast->delay(0, mockMillis, victimCallback);

    Timer::tick();
    assert(mutationFirstCalls == 1);
    assert(mutationTailCalls == 1);
    assert(mutationVictimCalls == 0);
    assert(childCalls == 0 && createdTimer != nullptr);

    Timer::tick();
    assert(childCalls == 1);

    delete first;
    delete middle;
    delete createdTimer;
    createdTimer = nullptr;
}

void testObserverMayDeleteCurrent() {
#if TIMER_STATIC_ENABLE_OBSERVER
    resetCounters();
    observerVictim = new Timer();
    observerVictim->delay(0, mockMillis, plainCallback);
    Timer::setTickObserver(deletingObserver);
    Timer::tick();
    Timer::setTickObserver(nullptr);
    assert(observerVictim == nullptr);
    assert(plainCalls == 0);
#endif
}

void testMoveConstructionPreservesListPosition() {
    {
        resetCounters();
        int order[3] = {};
        int count = 0;
        Timer first;
        Timer middle;
        Timer tail;
        first.delay_std(0, mockMillis, [&] { order[count++] = 1; });
        middle.delay_std(0, mockMillis, [&] { order[count++] = 2; });
        tail.delay_std(0, mockMillis, [&] { order[count++] = 3; });

        Timer moved(std::move(middle));
        assert(!middle.isRun());
        Timer::tick();
        assert(count == 3 && order[0] == 1 && order[1] == 2 && order[2] == 3);
    }
    {
        resetCounters();
        int order[2] = {};
        int count = 0;
        Timer head;
        Timer tail;
        head.delay_std(0, mockMillis, [&] { order[count++] = 1; });
        tail.delay_std(0, mockMillis, [&] { order[count++] = 2; });

        Timer moved(std::move(head));
        Timer::tick();
        assert(count == 2 && order[0] == 1 && order[1] == 2);
    }
    {
        resetCounters();
        int order[2] = {};
        int count = 0;
        Timer head;
        Timer tail;
        head.delay_std(0, mockMillis, [&] { order[count++] = 1; });
        tail.delay_std(0, mockMillis, [&] { order[count++] = 2; });

        Timer moved(std::move(tail));
        Timer::tick();
        assert(count == 2 && order[0] == 1 && order[1] == 2);
    }
}

void testMoveAssignmentReplacesDestinationNode() {
    resetCounters();
    int order[3] = {};
    int count = 0;
    Timer destination;
    Timer source;
    Timer tail;
    destination.delay_std(0, mockMillis, [&] { order[count++] = 9; });
    source.delay_std(0, mockMillis, [&] { order[count++] = 2; });
    tail.delay_std(0, mockMillis, [&] { order[count++] = 3; });

    destination = std::move(source);
    assert(!source.isRun());
    Timer::tick();
    assert(count == 2 && order[0] == 2 && order[1] == 3);

    Timer selfMove;
    selfMove.set(1, mockMillis, plainCallback);
    Timer* alias = &selfMove;
    selfMove = std::move(*alias);
    assert(selfMove.isRun());
    nowMs = 1;
    Timer::tick();
    assert(plainCalls == 1);
}

void testMoveCurrentAndNextDuringTick() {
    {
        resetCounters();
        int order[2] = {};
        int count = 0;
        Timer current;
        Timer tail;
        Timer* movedCurrent = nullptr;
        current.delay_std(0, mockMillis, [&] {
            order[count++] = 1;
            movedCurrent = new Timer(std::move(current));
        });
        tail.delay_std(0, mockMillis, [&] { order[count++] = 2; });

        Timer::tick();
        assert(movedCurrent != nullptr);
        assert(count == 2 && order[0] == 1 && order[1] == 2);
        delete movedCurrent;
    }
    {
        resetCounters();
        int order[3] = {};
        int count = 0;
        Timer first;
        Timer next;
        Timer tail;
        Timer* movedNext = nullptr;
        first.delay_std(0, mockMillis, [&] {
            order[count++] = 1;
            movedNext = new Timer(std::move(next));
        });
        next.delay_std(0, mockMillis, [&] { order[count++] = 2; });
        tail.delay_std(0, mockMillis, [&] { order[count++] = 3; });

        Timer::tick();
        assert(movedNext != nullptr);
        assert(count == 3 && order[0] == 1 && order[1] == 2 && order[2] == 3);
        delete movedNext;
    }
}

void testMoveOfDeferredTimerKeepsBoundary() {
    resetCounters();
    int order[3] = {};
    int count = 0;
    Timer parent;
    Timer tail;
    Timer* movedChild = nullptr;
    parent.delay_std(0, mockMillis, [&] {
        order[count++] = 1;
        Timer* child = new Timer();
        child->delay_std(0, mockMillis, [&] { order[count++] = 3; });
        movedChild = new Timer(std::move(*child));
        delete child;
    });
    tail.delay_std(0, mockMillis, [&] { order[count++] = 2; });

    Timer::tick();
    assert(movedChild != nullptr);
    assert(count == 2 && order[0] == 1 && order[1] == 2);
    Timer::tick();
    assert(count == 3 && order[2] == 3);
    delete movedChild;
}

void testMoveAssignmentDuringActiveCallback() {
    resetCounters();
    int order[2] = {};
    int count = 0;
    Timer current;
    Timer destination;
    Timer tail;
    current.delay_std(0, mockMillis, [&] {
        order[count++] = 1;
        destination = std::move(current);
    });
    destination.delay_std(0, mockMillis, [&] { order[count++] = 9; });
    tail.delay_std(0, mockMillis, [&] { order[count++] = 2; });

    Timer::tick();
    assert(count == 2 && order[0] == 1 && order[1] == 2);
}

void testMoveOfActivePeriodicStdCallbackRestoresCallable() {
    resetCounters();
    Timer source;
    Timer* moved = nullptr;
    int calls = 0;
    source.set_std(1, mockMillis, [&] {
        ++calls;
        if (calls == 1) moved = new Timer(std::move(source));
    });

    nowMs = 1;
    Timer::tick();
    assert(moved != nullptr && moved->isRun() && calls == 1);
    nowMs = 2;
    Timer::tick();
    assert(calls == 2);
    delete moved;
}

void testVectorReallocationDuringTickPreservesCursorAndBoundary() {
    resetCounters();
    int order[3] = {};
    int count = 0;
    std::vector<Timer> timers;
    timers.reserve(1);
    timers.emplace_back();
    Timer tail;
    timers.front().delay_std(0, mockMillis, [&] {
        order[count++] = 1;
        timers.emplace_back();
        timers.back().delay_std(0, mockMillis, [&] { order[count++] = 3; });
    });
    tail.delay_std(0, mockMillis, [&] { order[count++] = 2; });

    Timer::tick();
    assert(timers.size() == 2);
    assert(count == 2 && order[0] == 1 && order[1] == 2);
    Timer::tick();
    assert(count == 3 && order[2] == 3);
}

void testPoolReuseMoveAssignmentPreservesDeferredBoundary() {
    resetCounters();
    int order[3] = {};
    int count = 0;
    Timer parent;
    Timer recycledSlot;
    Timer following;
    parent.delay_std(0, mockMillis, [&] {
        order[count++] = 1;
        Timer localChild;
        localChild.delay_std(0, mockMillis, [&] { order[count++] = 3; });
        recycledSlot = std::move(localChild);
    });
    following.delay_std(0, mockMillis, [&] { order[count++] = 2; });

    Timer::tick();
    assert(count == 2 && order[0] == 1 && order[1] == 2);
    Timer::tick();
    assert(count == 3 && order[2] == 3);
}

void testWrapSafeQueries() {
    resetCounters();
    Timer timer;
    nowMs = std::numeric_limits<uint32_t>::max() - 5u;
    timer.set_std(10, mockMillis, [] { ++stdCalls; });
    assert(timer.getRemainingTime() == 10u);
    nowMs = std::numeric_limits<uint32_t>::max();
    assert(timer.getRemainingTime() == 5u);
    assert(!timer.isTimeExpired());
    nowMs = 4u;
    assert(timer.isTimeExpired());
    timer.check();
    assert(stdCalls == 1);
}

void testInvalidConfigurationAndExplicitStart() {
    resetCounters();
    Timer timer;
    timer.ON();
    assert(!timer.isRun());
    assert(!timer.resume());
    assert(!timer.restart());

    timer.delay(1, nullptr, plainCallback);
    assert(!timer.isRun());
    timer.set(1, mockMillis, static_cast<void (*)()>(nullptr));
    assert(!timer.isRun());

    timer.set(5, mockMillis, plainCallback);
    assert(timer.isRun());
    timer.OFF();
    assert(timer.resume());
    timer.OFF();
    nowMs = 3;
    assert(timer.restart());
    assert(timer.getRemainingTime() == 5);

    timer.setCallback(static_cast<void (*)()>(nullptr));
    assert(!timer.isRun());
    assert(!timer.resume());
    timer.setCallback(plainCallback);
    assert(!timer.isRun());
    assert(timer.resume());
    timer.setTimeFunc(nullptr);
    assert(!timer.isRun());
    assert(!timer.resume());

    Timer zeroCount;
    zeroCount.forCount(1, mockMillis, plainCallback, 0);
    zeroCount.ON();
    assert(!zeroCount.isRun());

    Timer zeroLifetime;
    zeroLifetime.forTime(1, mockMillis, plainCallback, 0);
    zeroLifetime.ON();
    assert(!zeroLifetime.isRun());

    Timer completed;
    completed.delay(0, mockMillis, plainCallback);
    Timer::tick();
    assert(!completed.isRun());
    assert(!completed.resume());
    assert(!completed.restart());
}

void testZeroPeriodPolicy() {
    resetCounters();
    Timer constructorTimer(0, mockMillis,
                           std::function<void()>([] { ++stdCalls; }));
    assert(!constructorTimer.isRun());

    Timer setTimer;
    setTimer.set(0, mockMillis, plainCallback);
    assert(!setTimer.isRun());

    Timer countTimer(reinterpret_cast<void*>(0x1234));
    countTimer.forCount(0, mockMillis, paramCallback, 2);
    assert(!countTimer.isRun());

    Timer timeTimer;
    timeTimer.forTime_std(0, mockMillis, [] { ++stdCalls; }, 10);
    assert(!timeTimer.isRun());

    Timer::tick();
    assert(plainCalls == 0 && paramCalls == 0 && stdCalls == 0);

    Timer zeroDelay;
    zeroDelay.delay(0, mockMillis, plainCallback);
    assert(zeroDelay.isRun());
    Timer::tick();
    assert(plainCalls == 1 && !zeroDelay.isRun());
    Timer::tick();
    assert(plainCalls == 1);

    Timer periodTimer;
    periodTimer.set(5, mockMillis, plainCallback);
    periodTimer.setPeriod(0);
    assert(!periodTimer.isRun());
    assert(!periodTimer.resume());
    periodTimer.setPeriod(2);
    assert(periodTimer.resume());
}

void testPlainParamAndStdSelfRearmThroughTick() {
    resetCounters();
    {
        Timer timer;
        selfRearmTimer = &timer;
        timer.delay(5, mockMillis, selfRearmCallback);

        nowMs = 5;
        Timer::tick();
        assert(plainCalls == 1 && timer.isRun());
        assert(timer.getRemainingTime() == 7);
        Timer::tick();
        assert(plainCalls == 1);
        nowMs = 12;
        Timer::tick();
        assert(plainCalls == 2 && !timer.isRun());
        selfRearmTimer = nullptr;
    }

    resetCounters();
    {
        ParamRearmState state;
        Timer timer(&state);
        state.timer = &timer;
        timer.delay(2, mockMillis, paramSelfRearmCallback);
        nowMs = 2;
        Timer::tick();
        assert(state.calls == 1 && timer.getRemainingTime() == 4);
        nowMs = 6;
        Timer::tick();
        assert(state.calls == 2 && !timer.isRun());
    }

    resetCounters();
    {
        Timer timer;
        int calls = 0;
        timer.delay_std(3, mockMillis, [&] {
            ++calls;
            timer.delay_std(5, mockMillis, [&] { ++calls; });
        });
        nowMs = 3;
        Timer::tick();
        assert(calls == 1 && timer.getRemainingTime() == 5);
        Timer::tick();
        assert(calls == 1);
        nowMs = 8;
        Timer::tick();
        assert(calls == 2 && !timer.isRun());
    }
}

void testPeriodChangePolicies() {
    resetCounters();
    {
        Timer timer;
        int calls = 0;
        timer.set_std(10, mockMillis, [&] {
            ++calls;
            if (calls == 1) timer.setPeriod(3);
        });
        nowMs = 10;
        Timer::tick();
        assert(calls == 1);
        assert(timer.getPeriod() == 3);
        assert(timer.getRemainingTime() == 10);
        nowMs = 19;
        Timer::tick();
        assert(calls == 1);
        nowMs = 20;
        Timer::tick();
        assert(calls == 2 && timer.getRemainingTime() == 3);
    }

    resetCounters();
    {
        Timer timer;
        int calls = 0;
        timer.set_std(10, mockMillis, [&] {
            ++calls;
            if (calls == 1) timer.restartWithPeriod(3);
        });
        nowMs = 10;
        Timer::tick();
        assert(calls == 1 && timer.getRemainingTime() == 3);
        nowMs = 12;
        Timer::tick();
        assert(calls == 1);
        nowMs = 13;
        Timer::tick();
        assert(calls == 2);
    }
}

void testZeroDelayReportsLastOnlyInsideCallback() {
    resetCounters();
    Timer timer;
    bool sawLast = false;
    timer.delay_std(0, mockMillis, [&] { sawLast = timer.isForLast(); });
    Timer::tick();
    assert(sawLast);
    assert(!timer.isForLast());
}

} // namespace

int main() {
    testCallbackTransitions();
    testConstantTimeCatchUp();
    testConstructorFirstFireCompatibility();
    testReentrantTickIsIgnored();
    testNewTimerWaitsForNextPass();
    testDeleteCurrentFromPlainAndStdCallbacks();
    testDeleteImmediateNextFromCallback();
    testDeleteOldLastAndDeferReplacement();
    testObserverMayDeleteCurrent();
    testMoveConstructionPreservesListPosition();
    testMoveAssignmentReplacesDestinationNode();
    testMoveCurrentAndNextDuringTick();
    testMoveOfDeferredTimerKeepsBoundary();
    testMoveAssignmentDuringActiveCallback();
    testMoveOfActivePeriodicStdCallbackRestoresCallable();
    testVectorReallocationDuringTickPreservesCursorAndBoundary();
    testPoolReuseMoveAssignmentPreservesDeferredBoundary();
    testWrapSafeQueries();
    testInvalidConfigurationAndExplicitStart();
    testZeroPeriodPolicy();
    testPlainParamAndStdSelfRearmThroughTick();
    testPeriodChangePolicies();
    testZeroDelayReportsLastOnlyInsideCallback();
    std::cout << "TimerStatic standalone tests: OK\n";
    return 0;
}
