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
  if (!t_func)
  {
    return;
  }
  uint32_t periodTmp = period;
  bool wasSetNewBefore = setNew;  // Сохраняем старое значение
  setNew = false;
  uint32_t now = t_func();
  
  if (isDue(now, nextTimeTrigger, periodTmp) && isRun_)
  {
#ifdef TIMER_DEBUG
    Serial.print("[Timer] Firing! now=");
    Serial.print(now);
    Serial.print(" next=");
    Serial.print(nextTimeTrigger);
    Serial.print(" period=");
    Serial.print(periodTmp);
    Serial.print(" wasSetNewBefore=");
    Serial.println(wasSetNewBefore);
#endif

    // ИСПРАВЛЕНИЕ: Проверяем остановку ДО callback
    bool isForTime = (!(this->isInf) && lifeShortener == lifeShortenerTime);
    uint32_t oldNextForTime = nextTimeTrigger;  // Сохраняем для вычисления прошедшего времени
    
    // forCount: проверяем счетчик ДО callback
    if (!(this->isInf) && lifeShortener == lifeShortenerCount)
    {
      uint32_t lifeShortenerVal = lifeShortener(this);
      if (this->life <= lifeShortenerVal)
      {
        this->isRun_ = false;
#ifdef TIMER_DEBUG
        Serial.println("[Timer] forCount: life expired, stopping");
#endif
        return;  // Останавливаем БЕЗ выполнения callback
      }
      this->life = lifeShortenerVal;
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

#ifdef TIMER_DEBUG
    Serial.print("[Timer] After callback: setNew=");
    Serial.print(setNew);
    Serial.print(" | will skip loop: ");
    Serial.println(wasSetNewBefore || setNew);
#endif

    // Пропускаем цикл если setNew был установлен ДО или ВНУТРИ callback
    if (wasSetNewBefore || setNew)
    {
      // forTime: если loop пропускается, вычитаем period
      if (isForTime)
      {
        if (this->life < periodTmp)
        {
          this->isRun_ = false;
#ifdef TIMER_DEBUG
          Serial.print("[forTime] STOP (skip loop): life(");
          Serial.print(this->life);
          Serial.print(") < period(");
          Serial.print(periodTmp);
          Serial.println(")");
#endif
          return;
        }
#ifdef TIMER_DEBUG
        Serial.print("[forTime] Skip loop: life ");
        Serial.print(this->life);
        Serial.print(" -= period ");
        Serial.print(periodTmp);
#endif
        this->life -= periodTmp;
#ifdef TIMER_DEBUG
        Serial.print(" → ");
        Serial.println(this->life);
#endif
      }
      
      // ИСПРАВЛЕНИЕ: ВСЕГДА поднимаем nextTimeTrigger в строго будущее при skip
      // чтобы избежать немедленного повторного срабатывания
      if (!setNew) // Только если setNew не был установлен в callback (иначе уже есть новое значение)
      {
        do {
          nextTimeTrigger += periodTmp;
          if (nextTimeTrigger < periodTmp) break; // overflow guard
        } while (periodTmp != 0 && (int32_t)(now - nextTimeTrigger) >= 0);
      }
      
#ifdef TIMER_DEBUG
      Serial.print("[Timer] Skipping loop (reset called). nextTrigger=");
      Serial.println(nextTimeTrigger);
#endif
      return;
    }
    
    // Обновляем nextTimeTrigger ПОСЛЕ выполнения callback'а
    uint32_t oldNext = nextTimeTrigger;
    do
    {
      nextTimeTrigger += periodTmp;
      
      // Проверка overflow: если после добавления меньше чем period, произошло переполнение
      if (nextTimeTrigger < periodTmp)
        break;
    } while (periodTmp != 0 && (int32_t)(now - nextTimeTrigger) >= 0);
    
    // forTime: вычисляем сколько "времени" прошло по таймеру ПОСЛЕ loop
    if (isForTime)
    {
      uint32_t timerElapsed = nextTimeTrigger - oldNextForTime;  // Сколько времени прошло ПО ТАЙМЕРУ
      
#ifdef TIMER_DEBUG
      Serial.print("[forTime] Timer elapsed = newNext(");
      Serial.print(nextTimeTrigger);
      Serial.print(") - oldNext(");
      Serial.print(oldNextForTime);
      Serial.print(") = ");
      Serial.println(timerElapsed);
      Serial.print("[forTime] life ");
      Serial.print(this->life);
      Serial.print(" -= ");
      Serial.print(timerElapsed);
#endif
      
      if (this->life < timerElapsed)
      {
        this->isRun_ = false;
#ifdef TIMER_DEBUG
        Serial.println(" → STOP (underflow)");
#endif
      }
      else
      {
        this->life -= timerElapsed;
#ifdef TIMER_DEBUG
        Serial.print(" → ");
        Serial.println(this->life);
#endif
      }
    }
    
#ifdef TIMER_DEBUG
    Serial.print("[Timer] Loop done: ");
    Serial.print(oldNext);
    Serial.print(" -> ");
    Serial.println(nextTimeTrigger);
#endif
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
  this->nextTimeTrigger = isPre ? t_func() - period : t_func() + period;
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
  this->nextTimeTrigger = isPre ? t_func() - period : t_func() + period;
}
#endif

bool Timer::isForLast() {
  // Проверяем полную инициализацию таймера
  if (isInf || t_func == nullptr || period == 0) {
    return false;
  }
  
  if (lifeShortener == lifeShortenerCount) {
    // Для forCount: последний если life == 0 (уже уменьшено перед callback)
    return life == 0;
  } else {
    // Для forTime: последний если осталось времени == 0 (уже уменьшено перед callback)
    return life == 0;
  }
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

