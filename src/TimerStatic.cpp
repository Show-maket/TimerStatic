#include "TimerStatic.h"

#ifndef __AVR__
#include <utility>
#endif

// Отладочный вывод (закомментируйте после отладки)
// #define TIMER_DEBUG 1

Timer *Timer::head = nullptr;
Timer *Timer::last = nullptr;
Timer *Timer::tickCursor = nullptr;
Timer *Timer::tickCurrent = nullptr;
Timer *Timer::tickBoundary = nullptr;
bool Timer::inTick = false;
#if TIMER_STATIC_ENABLE_OBSERVER
Timer::TickObserver Timer::tickObserver = nullptr;
#endif
Timer::CallbackFrame *Timer::callbackFrameTop = nullptr;

#if TIMER_STATIC_ENABLE_OBSERVER
void Timer::setTickObserver(TickObserver observer)
{
  Timer::tickObserver = observer;
}
#endif

void Timer::tick()
{
  // tick() is deliberately non-reentrant. Destruction advances the intrusive
  // cursor when necessary. tickBoundary is the first timer registered during
  // this pass, so newly-created timers are deferred without a heap snapshot.
  if (Timer::inTick) return;
  Timer::inTick = true;
  struct TickGuard {
    bool& flag;
    Timer *&cursor;
    Timer *&current;
    Timer *&boundary;
    ~TickGuard()
    {
      cursor = nullptr;
      current = nullptr;
      boundary = nullptr;
      flag = false;
    }
  } guard{Timer::inTick, Timer::tickCursor, Timer::tickCurrent,
          Timer::tickBoundary};

  Timer::tickCursor = Timer::head;
  Timer::tickBoundary = nullptr;
  while (Timer::tickCursor != nullptr &&
         Timer::tickCursor != Timer::tickBoundary)
  {
    Timer::tickCurrent = Timer::tickCursor;
    Timer::tickCursor = Timer::tickCurrent->next;
#if TIMER_STATIC_ENABLE_OBSERVER
    if (Timer::tickObserver != nullptr)
    {
      Timer::tickObserver(Timer::tickCurrent);
    }
    // The observer is allowed to destroy the current timer.
#endif
    if (Timer::tickCurrent != nullptr)
    {
      Timer::tickCurrent->check();
    }
    Timer::tickCurrent = nullptr;
  }
}

#ifndef __AVR__
Timer::CallbackGuard::CallbackGuard(
    Timer *timer, CallbackKind invokedKind,
    std::function<void()> *detachedStdCallback)
    : frame{timer, Timer::callbackFrameTop}, invokedKind(invokedKind),
      detachedStdCallback(detachedStdCallback)
{
  Timer::callbackFrameTop = &frame;
  timer->callbackActive = true;
}
#else
Timer::CallbackGuard::CallbackGuard(Timer *timer)
    : frame{timer, Timer::callbackFrameTop}
{
  Timer::callbackFrameTop = &frame;
  timer->callbackActive = true;
}
#endif

Timer::CallbackGuard::~CallbackGuard()
{
  Timer::callbackFrameTop = frame.previous;
  Timer *survivor = frame.timer;
  if (survivor == nullptr) return;

  survivor->callbackActive = false;
#ifndef __AVR__
  // Detaching keeps a currently executing std::function alive if its Timer is
  // deleted by that function. Restore only when it was not replaced/rearmed.
  if (invokedKind == CallbackKind::Std &&
      survivor->callbackKind == CallbackKind::Std &&
      !survivor->callbackStdFunc && detachedStdCallback != nullptr &&
      *detachedStdCallback)
  {
    survivor->callbackStdFunc = std::move(*detachedStdCallback);
  }
#endif
}

void Timer::selectCallback(CallbackFunc selected)
{
  callback = selected;
  callbackParam = nullptr;
#ifndef __AVR__
  callbackStdFunc = nullptr;
#endif
  callbackKind = selected != nullptr ? CallbackKind::Plain : CallbackKind::None;
  if (callbackKind == CallbackKind::None) isRun_ = false;
}

void Timer::selectCallback(CallbackFuncParam selected)
{
  callback = nullptr;
  callbackParam = selected;
#ifndef __AVR__
  callbackStdFunc = nullptr;
#endif
  callbackKind = selected != nullptr ? CallbackKind::Param : CallbackKind::None;
  if (callbackKind == CallbackKind::None) isRun_ = false;
}

#ifndef __AVR__
void Timer::selectCallback(std::function<void()> selected)
{
  callback = nullptr;
  callbackParam = nullptr;
  callbackStdFunc = std::move(selected);
  callbackKind = callbackStdFunc ? CallbackKind::Std : CallbackKind::None;
  if (callbackKind == CallbackKind::None) isRun_ = false;
}
#endif

bool Timer::hasValidCallback() const
{
  switch (callbackKind)
  {
    case CallbackKind::Plain:
      return callback != nullptr;
    case CallbackKind::Param:
      return callbackParam != nullptr;
#ifndef __AVR__
    case CallbackKind::Std:
      // The callable is intentionally detached while it is executing.
      return callbackActive || static_cast<bool>(callbackStdFunc);
#endif
    default:
      return false;
  }
}

bool Timer::isRunnableConfiguration() const
{
  if (t_func == nullptr || !hasValidCallback()) return false;
  if (period == 0 && !allowZeroPeriod) return false;
  if (!isInf && life == 0) return false;
  return true;
}

void Timer::_Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
{
  // period используется при вычислении nextTimeTrigger, поэтому инициализируем его ДО.
  // Важно: ранее здесь мог использоваться неинициализированный period.
  this->period = time;

  this->t_func = t_func;
  if (callbackP != nullptr) selectCallback(callbackP);
  this->isInf = true;
  this->allowZeroPeriod = false;
  this->isRun_ = isRunnableConfiguration();
  if (this->isRun_)
  {
    // Историческое поведение конструктора: при isPre=true запускать «как будто в прошлом».
    // При isPre=false — nextTimeTrigger = now (т.е. сработает при ближайшем tick()).
    this->nextTimeTrigger = isPre ? t_func() - this->period : t_func();
  }

  this->previous = Timer::last;
  if (Timer::last != nullptr)
  {
    Timer::last->next = this;
  }
  else
  {
    Timer::head = this;
  }
  Timer::last = this;
  this->linked = true;

  if (Timer::inTick && Timer::tickBoundary == nullptr)
  {
    Timer::tickBoundary = this;
  }
}

Timer::Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
{
  _Timer(time, t_func, callbackP, isPre);
}


#ifndef __AVR__
Timer::Timer(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre)
{
  selectCallback(std::move(callbackStd));
  _Timer(time, t_func, nullptr, isPre);
}
#else
Timer::Timer(unsigned long time, TimeFunc t_func, CallbackFunc callbackNoP, bool isPre)
{
  selectCallback(callbackNoP);
  _Timer(time, t_func, nullptr, isPre);
}
#endif

Timer::Timer(void *obj) : obj(obj)
{
  _Timer(0, nullptr, nullptr, false);
}

Timer::Timer()
{
  _Timer(0, nullptr, nullptr, false);
}

Timer::Timer(TimeFunc t_func, void *obj): obj(obj)
{
  _Timer(0, t_func, nullptr, false);
}

Timer::Timer(Timer&& other) noexcept
{
  adoptMovedState(other);
}

void Timer::adoptMovedState(Timer& other) noexcept
{
  // A Timer is an intrusive-list node.  Moving transfers that node identity
  // to the destination address instead of registering a second timer.
  previous = other.previous;
  next = other.next;
  nextTimeTrigger = other.nextTimeTrigger;
  period = other.period;
  t_func = other.t_func;
  callbackParam = other.callbackParam;
  callback = other.callback;
#ifndef __AVR__
  callbackStdFunc = std::move(other.callbackStdFunc);
#endif
  obj = other.obj;
  isRun_ = other.isRun_;
  isInf = other.isInf;
  life = other.life;
  lifeShortener = other.lifeShortener;
  linked = other.linked;
  callbackActive = other.callbackActive;
  allowZeroPeriod = other.allowZeroPeriod;
  callbackKind = other.callbackKind;

  if (linked)
  {
    if (previous != nullptr) previous->next = this;
    else Timer::head = this;

    if (next != nullptr) next->previous = this;
    else Timer::last = this;
  }

  // Moving is also safe while a tick/check stack is active.  Every scheduler
  // and callback-frame reference follows the logical timer to its new address.
  if (Timer::tickCursor == &other) Timer::tickCursor = this;
  if (Timer::tickCurrent == &other) Timer::tickCurrent = this;
  if (Timer::tickBoundary == &other) Timer::tickBoundary = this;
  for (CallbackFrame *frame = Timer::callbackFrameTop;
       frame != nullptr; frame = frame->previous)
  {
    if (frame->timer == &other) frame->timer = this;
  }

  // Leave a valid, inert moved-from object.  Its destructor must not unlink
  // the transferred node or invalidate the retargeted callback frame.
  other.previous = nullptr;
  other.next = nullptr;
  other.nextTimeTrigger = 0;
  other.period = 0;
  other.t_func = nullptr;
  other.callbackParam = nullptr;
  other.callback = nullptr;
#ifndef __AVR__
  other.callbackStdFunc = nullptr;
#endif
  other.obj = nullptr;
  other.isRun_ = false;
  other.isInf = true;
  other.life = 0;
  other.lifeShortener = Timer::lifeShortenerCount;
  other.linked = false;
  other.callbackActive = false;
  other.allowZeroPeriod = false;
  other.callbackKind = CallbackKind::None;
}

Timer& Timer::operator=(Timer&& other) noexcept
{
  if (this == &other) return *this;

  // Remove the destination's previous logical node without ending the C++
  // lifetime of the object, then transfer the source node into this storage.
  // This remains valid when Timer is a member or base subobject.
  detachLogicalNode();
  adoptMovedState(other);
  return *this;
}

uint32_t Timer::lifeShortenerCount(Timer *timer){ return timer->life - 1; }
uint32_t Timer::lifeShortenerTime(Timer *timer){
  if (timer->t_func == nullptr || timer->period == 0) {return 0; } // Таймер не может работать по времени
  return timer->life - (timer->period + ((timer->t_func() - timer->nextTimeTrigger) - timer->period));
}

// Защита от переполнения и джиттера
inline bool isDue(uint32_t now, uint32_t next) {
    return (int32_t)(now - next) >= 0;
}



void Timer::check()
{
  if (callbackActive || !isRun_)
  {
    return;
  }

  if (!isRunnableConfiguration())
  {
    isRun_ = false;
    return;
  }

  const uint32_t now = t_func();
  const uint32_t periodTmp = period;

  if (!isDue(now, nextTimeTrigger))
  {
    return;
  }

  // Определяем режим таймера
  const bool finite = !isInf;
  const bool isForCount = finite && (lifeShortener == lifeShortenerCount);
  const bool isForTime = finite && (lifeShortener == lifeShortenerTime);

  // Compute the next stable deadline in O(1). It is committed before user code;
  // resetToStart/resetToEnd from the callback then intentionally override it.
  const uint32_t oldNext = nextTimeTrigger;
  uint32_t nextCandidate = oldNext;
  uint64_t timerElapsed = 0;
  if (periodTmp == 0)
  {
    // period=0: «догонка» не нужна — nextCandidate = now (срабатываем на каждом check)
    nextCandidate = now;
    timerElapsed = static_cast<uint32_t>(now - oldNext);
  }
  else
  {
    // O(1) catch-up. The signed isDue() window above establishes that
    // `now - oldNext` is a valid non-negative lateness. Unsigned addition is
    // deliberately modulo 2^32, so this also works across millis() wrap.
    const uint32_t late = static_cast<uint32_t>(now - oldNext);
    const uint32_t steps = late / periodTmp + 1U;
    timerElapsed = static_cast<uint64_t>(steps) * periodTmp;
    nextCandidate = oldNext + static_cast<uint32_t>(timerElapsed);
  }

  // === life -> ВАЖНО: life обновляется ДО callback, чтобы isForLast() работал внутри callback ===
  if (isForCount)
  {
    if (life == 0)
    {
      isRun_ = false;
      return;
    }
    // На последнем вызове life станет 0 ещё ДО callback
    life -= 1;
  }
  else if (isForTime)
  {
    if (life == 0)
    {
      isRun_ = false;
      return;
    }

    // Важно: сохраняем историческое поведение forTime — коллбэк вызывается даже если life не хватает
    // на полный шаг. В этом случае это последний вызов (life принудительно становится 0).
    if (life <= timerElapsed)
    {
      life = 0;
    }
    else
    {
      life -= timerElapsed;
    }
  }

  // Commit the default post-fire state before entering user code. From this
  // point onward the callback may reconfigure or destroy *this*, so check()
  // must not access any Timer member after invocation starts.
  nextTimeTrigger = nextCandidate;
  if (finite && life == 0)
  {
    isRun_ = false;
  }

  const CallbackKind invokedKind = callbackKind;
  const CallbackFunc invokedCallback = callback;
  const CallbackFuncParam invokedCallbackParam = callbackParam;
  void *const invokedObj = obj;
#ifndef __AVR__
  std::function<void()> detachedStdCallback;
  if (invokedKind == CallbackKind::Std)
  {
    detachedStdCallback = std::move(callbackStdFunc);
  }
  CallbackGuard callbackGuard(this, invokedKind, &detachedStdCallback);
#else
  CallbackGuard callbackGuard(this);
#endif

  switch (invokedKind)
  {
    case CallbackKind::Plain:
      invokedCallback();
      break;
    case CallbackKind::Param:
      invokedCallbackParam(invokedObj);
      break;
#ifndef __AVR__
    case CallbackKind::Std:
      detachedStdCallback();
      break;
#endif
    default:
      break;
  }
}

void Timer::delay(uint32_t time, TimeFunc t_func, CallbackFunc callback)
{
  selectCallback(callback);
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = true;
  this->t_func = t_func;
  this->life = 1;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? t_func() + time : 0;
}

void Timer::delay(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP)
{
  selectCallback(callbackP);
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = true;
  this->t_func = t_func;
  this->life = 1;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? t_func() + time : 0;
}

#ifndef __AVR__
void Timer::delay_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd)
{
  selectCallback(std::move(callbackStd));

  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = true;
  this->t_func = t_func;
  this->life = 1;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? t_func() + time : 0;
}
#endif

void Timer::forCount(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint16_t lifeCount, bool isPre)
{
  selectCallback(callback);
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = lifeCount;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() - period : t_func() + period) : 0;
}

void Timer::forCount(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint16_t lifeCount, bool isPre)
{
  selectCallback(callbackP);
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = lifeCount;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() - period : t_func() + period) : 0;
}

#ifndef __AVR__
void Timer::forCount_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint16_t lifeCount, bool isPre)
{
  selectCallback(std::move(callbackStd));

  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = lifeCount;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() - period : t_func() + period) : 0;
}
#endif

void Timer::forTime(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint32_t lifeTime, bool isPre)
{
  selectCallback(callback);
  this->lifeShortener = Timer::lifeShortenerTime;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = lifeTime;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() : t_func() + period) : 0;
}

void Timer::forTime(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint32_t lifeTime, bool isPre)
{
  selectCallback(callbackP);
  this->lifeShortener = Timer::lifeShortenerTime;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = lifeTime;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  // В forTime isPre означает «вызвать callback как можно скорее»,
  // но НЕ сдвигать внутренний график на 2 периода (как при now - period).
  // Поэтому стартуем с nextTimeTrigger = now.
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() : t_func() + period) : 0;
}

#ifndef __AVR__
void Timer::forTime_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint32_t lifeTime, bool isPre)
{
  selectCallback(std::move(callbackStd));

  this->lifeShortener = Timer::lifeShortenerTime;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = lifeTime;
  this->isInf = false;
  this->isRun_ = isRunnableConfiguration();
  // См. комментарий в forTime(..., CallbackFuncParam, ...)
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() : t_func() + period) : 0;
}
#endif

bool Timer::isForLast()
{
  // Finite timers commit their completed/stopped state before callback entry,
  // so callbackActive (not isRun_) defines the only valid observation window.
  return callbackActive && !isInf && life == 0;
}

void Timer::setLifeCount(uint16_t newLifeCount) {
  if (isInf || (!isRun_ && !callbackActive)) return;
  this->life = newLifeCount;
  if (newLifeCount == 0) this->isRun_ = false;
  else if (callbackActive) this->isRun_ = isRunnableConfiguration();
}

void Timer::setLifeTime(uint32_t newLifeTime) {
  if (isInf || (!isRun_ && !callbackActive)) return;
  this->life = newLifeTime;
  if (newLifeTime == 0) this->isRun_ = false;
  else if (callbackActive) this->isRun_ = isRunnableConfiguration();
}

bool Timer::resume()
{
  this->isRun_ = isRunnableConfiguration();
  return this->isRun_;
}

bool Timer::restart()
{
  if (!resume()) return false;
  resetToStart();
  return true;
}

void Timer::setTimeFunc(TimeFunc tFunc)
{
  this->t_func = tFunc;
  if (!isRunnableConfiguration()) this->isRun_ = false;
}

void Timer::setPeriod(uint32_t val)
{
  this->period = val;
  if (!isRunnableConfiguration()) this->isRun_ = false;
}

void Timer::set(unsigned long time, TimeFunc t_func, CallbackFunc callback, bool isPre)
{
  selectCallback(callback);
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = 0;
  this->isInf = true;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() - period : t_func() + period) : 0;
}

void Timer::set(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
{
  selectCallback(callbackP);
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = 0;
  this->isInf = true;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() - period : t_func() + period) : 0;
}

#ifndef __AVR__
void Timer::set_std(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre)
{
  selectCallback(std::move(callbackStd));

  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->allowZeroPeriod = false;
  this->t_func = t_func;
  this->life = 0;
  this->isInf = true;
  this->isRun_ = isRunnableConfiguration();
  this->nextTimeTrigger = this->isRun_ ? (isPre ? t_func() - period : t_func() + period) : 0;
}
#endif

void Timer::setCallback(CallbackFunc callback)
{
  selectCallback(callback);
}

void Timer::setCallback(CallbackFuncParam callbackP)
{
  selectCallback(callbackP);
}

#ifndef __AVR__
void Timer::setCallback_std(std::function<void()> func)
{
  selectCallback(std::move(func));
}
#endif

void Timer::unlinkFromList() noexcept {
  if (!linked) return;

  if (previous != nullptr) previous->next = next;
  else Timer::head = next;

  if (next != nullptr) next->previous = previous;
  else Timer::last = previous;

  linked = false;
  previous = nullptr;
  this->next = nullptr;
}

void Timer::detachLogicalNode() noexcept {
  // Destruction is legal from an observer or callback. Invalidate every live
  // stack frame that could otherwise touch this object while unwinding.
  for (CallbackFrame *frame = Timer::callbackFrameTop;
       frame != nullptr; frame = frame->previous)
  {
    if (frame->timer == this) frame->timer = nullptr;
  }

  if (Timer::tickCurrent == this) Timer::tickCurrent = nullptr;
  if (Timer::tickCursor == this) Timer::tickCursor = this->next;
  if (Timer::tickBoundary == this) Timer::tickBoundary = this->next;
  unlinkFromList();
  callbackActive = false;
}

Timer::~Timer() {
  detachLogicalNode();
}

// Геттеры для получения оставшегося времени
unsigned long Timer::getRemainingTime() const {
  if (!isRun_ || t_func == nullptr) return 0;
  const uint32_t currentTime = static_cast<uint32_t>(t_func());
  const int32_t remaining = static_cast<int32_t>(nextTimeTrigger - currentTime);
  return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
}

bool Timer::isTimeExpired() const {
  if (!isRun_ || t_func == nullptr) return true;
  const uint32_t currentTime = static_cast<uint32_t>(t_func());
  return static_cast<int32_t>(currentTime - nextTimeTrigger) >= 0;
}

