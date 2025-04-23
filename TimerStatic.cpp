#include "TimerStatic.h"

Timer *Timer::head = nullptr;
Timer *Timer::last = nullptr;

void Timer::tick()
{
  Timer *current = Timer::head;
  while (current != nullptr)
  {
    current->check();
    current = current->next;
  }
}

void Timer::_Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
{
  if (Timer::head == nullptr)
  {
    Timer::head = this;
  }
  if (last != nullptr)
  {
    last->next = this;
  }
  last = this;

  this->t_func = t_func;
  this->isInf = true;
  if (!(callback == nullptr || callbackParam == nullptr) || !t_func)
  {
    isRun_ = false;
  }
  else
  {
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }
  this->period = time;
  this->callbackParam = callbackP;
}

Timer::Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
{
  dontUseParam = 0;
  _Timer(time, t_func, callbackP, isPre);
}

Timer::Timer(unsigned long time, TimeFunc t_func, CallbackFunc callbackNoP, bool isPre)
{
  dontUseParam = 2;
  this->callback = callbackNoP;
  _Timer(time, t_func, nullptr, isPre);
}

#ifndef __AVR__
Timer::Timer(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre)
{
  dontUseParam = 0;
  this->callbackStdFunc = callbackStd;
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

uint32_t Timer::lifeShortenerCount(Timer *timer){return timer->life - 1;}
uint32_t Timer::lifeShortenerTime(Timer *timer){return timer->life - (timer->period + ((timer->t_func() - timer->nextTimeTrigger) - timer->period));}

void Timer::check()
{
  if (!t_func)
  {
    return;
  }
  uint32_t periodTmp = period;
  setNew = false;
  if (t_func() - nextTimeTrigger >= periodTmp && isRun_)
  {
#ifndef __AVR__
    if (callbackStdFunc)
    {
      callbackStdFunc();
    }
    else
#endif
    if (dontUseParam)
    {
      this->callback();
    }
    else
    {
      callbackParam(obj);
    }

    if (setNew)
    {
      return;
    }
    uint32_t lifeShortenerVal = lifeShortener(this);
    if (!(this->isInf) && this->life < lifeShortenerVal)
    {
      this->isRun_ = false;
    }

    this->life = lifeShortenerVal;
    do
    {
      nextTimeTrigger += periodTmp;
      if (nextTimeTrigger < periodTmp)
        break;
    } while (periodTmp != 0 && nextTimeTrigger < t_func() - periodTmp);
  }
}

void Timer::delay(uint32_t time, TimeFunc t_func, CallbackFunc callback)
{
  this->callback = callback;
  dontUseParam = 2;
  delay(time, t_func, [](void *) {});
}

void Timer::delay(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP)
{
  if (dontUseParam)
  {
    dontUseParam--;
  }
  this->callbackParam = callbackP;
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->t_func = t_func;
  this->life = 0;
  this->isRun_ = true;
  this->isInf = false;
  this->setNew = true;
  this->nextTimeTrigger = t_func();
}

#ifndef __AVR__
void Timer::delay(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd)
{
  this->callbackStdFunc = callbackStd;
  this->callback = nullptr;
  this->callbackParam = nullptr;
  dontUseParam = 0;

  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->t_func = t_func;
  this->life = 0;
  this->isRun_ = true;
  this->isInf = false;
  this->setNew = true;
  this->nextTimeTrigger = t_func();
}
#endif

void Timer::forCount(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint16_t lifeCount, bool isPre)
{
  this->callback = callback;
  dontUseParam = 2;
  forCount(
      time, t_func, [](void *) {}, lifeCount, isPre);
}

void Timer::forCount(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint16_t lifeCount, bool isPre)
{
  if (dontUseParam)
  {
    dontUseParam--;
  }
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->t_func = t_func;
  this->callbackParam = callbackP;
  this->life = lifeCount;
  this->isInf = false;
  this->isRun_ = lifeCount != 0;
  this->setNew = true;
  this->nextTimeTrigger = isPre ? t_func() - period : t_func();
}

#ifndef __AVR__
void Timer::forCount(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint16_t lifeCount, bool isPre)
{
  this->callbackStdFunc = callbackStd;
  this->callback = nullptr;
  this->callbackParam = nullptr;
  dontUseParam = 0;

  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->t_func = t_func;
  this->life = lifeCount;
  this->isInf = false;
  this->isRun_ = lifeCount != 0;
  this->setNew = true;
  this->nextTimeTrigger = isPre ? t_func() - period : t_func();
}
#endif

void Timer::forTime(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint32_t lifeTime, bool isPre)
{
  this->callback = callback;
  dontUseParam = 2;
  forTime(
      time, t_func, [](void *) {}, lifeTime, isPre);
}

void Timer::forTime(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint32_t lifeTime, bool isPre)
{
  if (dontUseParam)
  {
    dontUseParam--;
  }
  this->lifeShortener = Timer::lifeShortenerTime;
  this->period = time;
  this->t_func = t_func;
  this->callbackParam = callbackP;
  this->life = lifeTime;
  this->isInf = false;
  this->isRun_ = lifeTime != 0;
  this->setNew = true;
  this->nextTimeTrigger = isPre ? t_func() - period : t_func();
}

#ifndef __AVR__
void Timer::forTime(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint32_t lifeTime, bool isPre)
{
  this->callbackStdFunc = callbackStd;
  this->callback = nullptr;
  this->callbackParam = nullptr;
  dontUseParam = 0;

  this->lifeShortener = Timer::lifeShortenerTime;
  this->period = time;
  this->t_func = t_func;
  this->life = lifeTime;
  this->isInf = false;
  this->isRun_ = lifeTime != 0;
  this->setNew = true;
  this->nextTimeTrigger = isPre ? t_func() - period : t_func();
}
#endif

bool Timer::isForLast()
{
  return life < lifeShortener(this);
}

void Timer::set(unsigned long time, TimeFunc t_func, CallbackFunc callback, bool isPre)
{
  this->callback = callback;
  dontUseParam = 2;
  set(
      time, t_func, [](void *) {}, isPre);
}

void Timer::set(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
{
  if (dontUseParam)
  {
    dontUseParam--;
  }
  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->t_func = t_func;
  this->callbackParam = callbackP;
  this->life = 0;
  this->isInf = true;
  this->isRun_ = true;
  this->setNew = true;
  this->nextTimeTrigger = isPre ? t_func() - period : t_func();
}

#ifndef __AVR__
void Timer::set(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre)
{
  this->callbackStdFunc = callbackStd;
  this->callback = nullptr;
  this->callbackParam = nullptr;
  dontUseParam = 0;

  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->t_func = t_func;
  this->life = 0;
  this->isInf = true;
  this->isRun_ = true;
  this->setNew = true;
  this->nextTimeTrigger = isPre ? t_func() - period : t_func();
}
#endif

void Timer::setCallback(CallbackFunc callback)
{
  dontUseParam = 2;
  this->callback = callback;
}

void Timer::setCallback(CallbackFuncParam callbackP)
{
  if (dontUseParam)
  {
    dontUseParam--;
  }
  this->callbackParam = callbackP;
}

#ifndef __AVR__
void Timer::setCallback(std::function<void()> func)
{
  callbackStdFunc = func;
  callback = nullptr;
  callbackParam = nullptr;
  dontUseParam = 0;
}
#endif

Timer::~Timer() {
  if (head == nullptr) {
      return;
  }
  if (this == head) {
      head = this->next;
      if (this == last) {
          last = nullptr;
      }
  } else {
      Timer* prev = head;
      while (prev != nullptr && prev->next != this) {
          prev = prev->next;
      }
      if (prev != nullptr) {
          prev->next = this->next;
      }
      if (this == last) {
          last = prev;
      }
  }
  this->next = nullptr;
}

Timer::Timer(Timer&& other) noexcept {
  this->~Timer();
  this->next = other.next;
  this->nextTimeTrigger = other.nextTimeTrigger;
  this->period = other.period;
  this->t_func = other.t_func;
  this->callback = other.callback;
  this->callbackParam = other.callbackParam;
#ifndef __AVR__
  this->callbackStdFunc = std::move(other.callbackStdFunc);
#endif
  this->obj = other.obj;
  this->isRun_ = other.isRun_;
  this->isInf = other.isInf;
  this->life = other.life;
  this->lifeShortener = other.lifeShortener;
  this->setNew = other.setNew;
  this->dontUseParam = other.dontUseParam;

  other.next = nullptr;
  other.t_func = nullptr;
  other.callback = nullptr;
  other.callbackParam = nullptr;
  other.obj = nullptr;
  other.isRun_ = false;
  other.life = 0;
  other.setNew = false;
#ifndef __AVR__
  other.callbackStdFunc = nullptr;
#endif

  if (Timer::head == &other) {
    Timer::head = this;
  } else {
    Timer* prev = Timer::head;
    while (prev && prev->next != &other) {
      prev = prev->next;
    }
    if (prev) {
      prev->next = this;
    }
  }
  if (Timer::last == &other) {
    Timer::last = this;
  }
}

Timer& Timer::operator=(Timer&& other) noexcept {
  if (this != &other) {
    this->~Timer();
    this->next = other.next;
    this->nextTimeTrigger = other.nextTimeTrigger;
    this->period = other.period;
    this->t_func = other.t_func;
    this->callback = other.callback;
    this->callbackParam = other.callbackParam;
#ifndef __AVR__
    this->callbackStdFunc = std::move(other.callbackStdFunc);
#endif
    this->obj = other.obj;
    this->isRun_ = other.isRun_;
    this->isInf = other.isInf;
    this->life = other.life;
    this->lifeShortener = other.lifeShortener;
    this->setNew = other.setNew;
    this->dontUseParam = other.dontUseParam;

    other.next = nullptr;
    other.t_func = nullptr;
    other.callback = nullptr;
    other.callbackParam = nullptr;
    other.obj = nullptr;
    other.isRun_ = false;
    other.life = 0;
    other.setNew = false;
#ifndef __AVR__
    other.callbackStdFunc = nullptr;
#endif

    if (Timer::head == &other) {
      Timer::head = this;
    } else {
      Timer* prev = Timer::head;
      while (prev && prev->next != &other) {
        prev = prev->next;
      }
      if (prev) {
        prev->next = this;
      }
    }
    if (Timer::last == &other) {
      Timer::last = this;
    }
  }
  return *this;
}
