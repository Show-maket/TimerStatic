#include "TimerStatic.h"

// Отладочный вывод (закомментируйте после отладки)
// #define TIMER_DEBUG 1

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

  // period используется при вычислении nextTimeTrigger, поэтому инициализируем его ДО.
  // Важно: ранее здесь мог использоваться неинициализированный period.
  this->period = time;

  this->t_func = t_func;
  this->isInf = true;
  if (!(callback == nullptr || callbackParam == nullptr) || !t_func)
  {
    isRun_ = false;
  }
  else
  {
    // Историческое поведение конструктора: при isPre=true запускать «как будто в прошлом».
    // При isPre=false — nextTimeTrigger = now (т.е. сработает при ближайшем tick()).
    this->nextTimeTrigger = isPre ? t_func() - this->period : t_func();
  }
  this->callbackParam = callbackP;
}

Timer::Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
{
  dontUseParam = 0;
  _Timer(time, t_func, callbackP, isPre);
}


#ifndef __AVR__
Timer::Timer(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre)
{
  dontUseParam = 0;
  this->callbackStdFunc = callbackStd;
  _Timer(time, t_func, nullptr, isPre);
}
#else
Timer::Timer(unsigned long time, TimeFunc t_func, CallbackFunc callbackNoP, bool isPre)
{
  dontUseParam = 2;
  this->callback = callbackNoP;
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

uint32_t Timer::lifeShortenerCount(Timer *timer){ return timer->life - 1; }
uint32_t Timer::lifeShortenerTime(Timer *timer){
  if (timer->t_func == nullptr || timer->period == 0) {return 0; } // Таймер не может работать по времени
  return timer->life - (timer->period + ((timer->t_func() - timer->nextTimeTrigger) - timer->period));
}

// Защита от переполнения и джиттера
inline bool isDue(uint32_t now, uint32_t next, uint32_t period) {
    return (int32_t)(now - next) >= 0;
}



void Timer::check()
{
  if (!t_func || !isRun_)
  {
    return;
  }

  const uint32_t now = t_func();
  const uint32_t periodTmp = period;
  if (periodTmp == 0)
  {
    // Нулевой период приводит к бесконечному циклу догонки.
    return;
  }

  // setNew используется как внутренний флаг «nextTimeTrigger был изменён пользователем/коллбэком».
  // Он не должен «залипать» между тиками (иначе ломает логику догонки), поэтому очищаем его здесь.
  setNew = false;

  if (!isDue(now, nextTimeTrigger, periodTmp))
  {
    return;
  }

  // Определяем режим таймера
  const bool finite = !isInf;
  const bool isForCount = finite && (lifeShortener == lifeShortenerCount);
  const bool isForTime = finite && (lifeShortener == lifeShortenerTime);

  // Считаем, каким будет следующий nextTimeTrigger (догоняем в строго будущее),
  // но сам nextTimeTrigger пока НЕ меняем — это важно, чтобы уважать resetToStart/resetToEnd из callback.
  const uint32_t oldNext = nextTimeTrigger;
  uint32_t nextCandidate = oldNext;
  do
  {
    nextCandidate += periodTmp;
    if (nextCandidate < periodTmp)
    {
      // overflow guard
      break;
    }
  } while (isDue(now, nextCandidate, periodTmp));

  // Сколько «таймерного времени» должно быть списано за этот тик.
  // В normal-case это periodTmp. Если tick() опоздал и пропущены интервалы — это k*periodTmp.
  const uint32_t timerElapsed = nextCandidate - oldNext;

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

  // Если коллбэк НЕ делал resetToStart/resetToEnd, то применяем рассчитанное расписание.
  if (!setNew)
  {
    nextTimeTrigger = nextCandidate;
  }

  // Финитные таймеры (forCount/forTime/delay) автоматически останавливаются сразу после
  // «последнего» коллбэка, если пользователь внутри callback не продлил life.
  if (!isInf && life == 0)
  {
    isRun_ = false;
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
  this->life = 1;  // ИСПРАВЛЕНИЕ: должен сработать 1 раз
  this->isRun_ = true;
  this->isInf = false;
  this->setNew = true;
  this->nextTimeTrigger = t_func() + time;
}

#ifndef __AVR__
void Timer::delay_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd)
{
  this->callbackStdFunc = callbackStd;
  this->callback = nullptr;
  this->callbackParam = nullptr;
  dontUseParam = 0;

  this->lifeShortener = Timer::lifeShortenerCount;
  this->period = time;
  this->t_func = t_func;
  this->life = 1;  // ИСПРАВЛЕНИЕ: должен сработать 1 раз
  this->isRun_ = true;
  this->isInf = false;
  this->setNew = true;
  this->nextTimeTrigger = t_func() + time;
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
  this->nextTimeTrigger = isPre ? t_func() - period : t_func() + period;
}

#ifndef __AVR__
void Timer::forCount_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint16_t lifeCount, bool isPre)
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
  this->nextTimeTrigger = isPre ? t_func() - period : t_func() + period;
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
  // В forTime isPre означает «вызвать callback как можно скорее»,
  // но НЕ сдвигать внутренний график на 2 периода (как при now - period).
  // Поэтому стартуем с nextTimeTrigger = now.
  this->nextTimeTrigger = isPre ? t_func() : t_func() + period;
}

#ifndef __AVR__
void Timer::forTime_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint32_t lifeTime, bool isPre)
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
  // См. комментарий в forTime(..., CallbackFuncParam, ...)
  this->nextTimeTrigger = isPre ? t_func() : t_func() + period;
}
#endif

bool Timer::isForLast()
{
  // Гарантия:
  // - Для forCount/forTime/delay: isForLast()==true ТОЛЬКО в том callback, который является последним.
  // - После завершения (isRun_==false) isForLast() всегда false.
  if (!isRun_ || isInf || t_func == nullptr || period == 0)
  {
    return false;
  }
  return life == 0;
}

void Timer::setLifeCount(uint16_t newLifeCount) {
    if (!isRun_ || isInf) return;
    this->life = newLifeCount;
}

void Timer::setLifeTime(uint32_t newLifeTime) {
    if (!isRun_ || isInf) return;
    this->life = newLifeTime;
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
  this->nextTimeTrigger = isPre ? t_func() - period : t_func() + period;
}

#ifndef __AVR__
void Timer::set_std(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre)
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
  this->nextTimeTrigger = isPre ? t_func() - period : t_func() + period;
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
void Timer::setCallback_std(std::function<void()> func)
{
  callbackStdFunc = func;
  callback = nullptr;
  callbackParam = nullptr;
  dontUseParam = 0;
}
#endif

void Timer::unlinkFromList() {
  if (Timer::head == nullptr) return;

  if (this == Timer::head) {
    Timer::head = this->next;
    if (this == Timer::last) Timer::last = nullptr;
  } else {
    Timer* prev = Timer::head;
    while (prev && prev->next != this) prev = prev->next;
    if (prev) prev->next = this->next;
    if (this == Timer::last) Timer::last = prev;
  }

  this->next = nullptr;
}

Timer::~Timer() {
  unlinkFromList();
}

Timer::Timer(Timer&& other) noexcept {
  unlinkFromList();  // удалить this из списка, если он был связан

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

  // Обновление связей в глобальном списке
  if (Timer::head == &other) {
    Timer::head = this;
  } else {
    Timer* prev = Timer::head;
    while (prev && prev->next != &other) prev = prev->next;
    if (prev) prev->next = this;
  }

  if (Timer::last == &other) {
    Timer::last = this;
  }
}


Timer& Timer::operator=(Timer&& other) noexcept {
  if (this != &other) {
    unlinkFromList();  // удалить this из списка перед перемещением

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

    // Обновление связей в глобальном списке
    if (Timer::head == &other) {
      Timer::head = this;
    } else {
      Timer* prev = Timer::head;
      while (prev && prev->next != &other) prev = prev->next;
      if (prev) prev->next = this;
    }

    if (Timer::last == &other) {
      Timer::last = this;
    }
  }
  return *this;
}

// Геттеры для получения оставшегося времени
unsigned long Timer::getRemainingTime() const {
  if (!isRun_ || t_func == nullptr) return 0;
  unsigned long currentTime = t_func();
  if (nextTimeTrigger > currentTime) {
    return nextTimeTrigger - currentTime;
  }
  return 0;
}

bool Timer::isTimeExpired() const {
  if (!isRun_ || t_func == nullptr) return true;
  return t_func() >= nextTimeTrigger;
}

