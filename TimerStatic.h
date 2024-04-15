#pragma once
class Timer
{
private:
  static Timer *head;
  static Timer *last;

  typedef void (*CallbackFunc)();
  typedef void (*CallbackFuncParam)(void *);
  typedef unsigned long (*TimeFunc)();
  typedef uint32_t (*LifeShortenerFunc)(Timer *t);

public:
  static void tick()
  {
    Timer *current = Timer::head;
    while (current != nullptr)
    {
      current->check();
      current = current->next;
    }
  }

private:
  Timer *next;
  unsigned long nextTimeTrigger, period;
  TimeFunc t_func;
  CallbackFuncParam callbackParam;
  CallbackFunc callback;
  void *obj = nullptr;
  bool isRun = true;
  bool isInf;
  uint32_t life = 0;
  LifeShortenerFunc lifeShortener = Timer::lifeShortenerCount;
  bool setNew = false;
  uint8_t dontUseParam = 0;

  static inline uint32_t lifeShortenerCount(Timer *timer)
  {
    return timer->life - 1;
  }
  static inline uint32_t lifeShortenerTime(Timer *timer)
  {
    return timer->life - (timer->period + ((timer->t_func() - timer->nextTimeTrigger) - timer->period));
  }

  void _Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre)
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
      isRun = false;
    }
    else
    {
      this->nextTimeTrigger = isPre ? t_func() - period : t_func();
    }
    this->period = time;
    this->callbackParam = callbackP;
  }

public:
  Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre = false)
  {
    dontUseParam = 0;
    _Timer(time, t_func, callbackP, isPre);
  }
  Timer(unsigned long time, TimeFunc t_func, CallbackFunc callbackNoP, bool isPre = false)
  {
    dontUseParam = 2;
    this->callback = callbackNoP;
    _Timer(time, t_func, nullptr, isPre);
  }
  Timer(void *obj) : obj(obj)
  {
    _Timer(0, nullptr, nullptr, false);
  }
  Timer()
  {
    _Timer(0, nullptr, nullptr, false);
  }

  void setObj(void *obj)
  {
    this->obj = obj;
  }

  void check()
  {
    if (!t_func)
    {
      return;
    }
    uint32_t periodTmp = period;
    setNew = false;
    if (t_func() - nextTimeTrigger >= periodTmp && isRun)
    {

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
        this->isRun = false;
        // Serial.println("TIMER STOP");
      }

      this->life = lifeShortenerVal;
      do
      {
        nextTimeTrigger += periodTmp;
        if (nextTimeTrigger < periodTmp)
          break;                                                          // переполнение uint32_t
      } while (periodTmp != 0 && nextTimeTrigger < t_func() - periodTmp); // защита от пропуска шага
    }
  }

  void resetToStart()
  {
    nextTimeTrigger = t_func();
  }
  void resetToEnd()
  {
    nextTimeTrigger = t_func() - period;
  }

  void ON()
  {
    this->isRun = true;
  }
  void OFF()
  {
    this->isRun = false;
  }

  void delay(uint32_t time, TimeFunc t_func, CallbackFunc callback)
  {
    this->callback = callback;
    dontUseParam = 2;
    delay(time, t_func, [](void *) {});
  }
  void delay(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP)
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
    this->isRun = true;
    this->isInf = false;
    this->setNew = true;
    this->nextTimeTrigger = t_func();

    // Serial.println("S");
  }

  void forCount(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint16_t lifeCount, bool isPre = true)
  {
    this->callback = callback;
    dontUseParam = 2;
    forCount(
        time, t_func, [](void *) {}, lifeCount, isPre);
  }
  void forCount(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint16_t lifeCount, bool isPre = true)
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
    this->isRun = lifeCount != 0;
    // this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

  void forTime(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint32_t lifeTime, bool isPre = true)
  {
    this->callback = callback;
    dontUseParam = 2;
    forTime(
        time, t_func, [](void *) {}, lifeTime, isPre);
  }
  void forTime(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint32_t lifeTime, bool isPre = true)
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
    this->isRun = lifeTime != 0;
    this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

  bool isForLast()
  {
    return life < lifeShortener(this);
  }

  void set(unsigned long time, TimeFunc t_func, CallbackFunc callback, bool isPre = false)
  {
    this->callback = callback;
    dontUseParam = 2;
    set(
        time, t_func, [](void *) {}, isPre);
  }
  void set(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre = false)
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
    this->isRun = true;
    this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

  void setPeriod(uint32_t val)
  {
    this->period = val;
  }
  uint32_t getPeriod()
  {
    return this->period;
  }

  void setCallback(CallbackFunc callback)
  {
    dontUseParam = 2;
    this->callback = callback;
  }

  void setCallback(CallbackFuncParam callbackP)
  {
    if (dontUseParam)
    {
      dontUseParam--;
    }
    this->callbackParam = callbackP;
  }
};

Timer* Timer::head;
Timer* Timer::last = nullptr;

/*///////////////////////////////////////////////////////////////////////////////////////////////////////

  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//***************************************************
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀Product⠀⠀⠀⠀⣾⠙⠻⢶⣄⡀⠀⠀⠀⢀⣤⠶⠛⠛⡇⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀by DashyFox⠀⠀⠀⢹⣇⠀⠀⣙⣿⣦⣤⣴⣿⣁⠀⠀⣸⠇⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀≽^ܫ^≼⠀⠀⠀⠀⠀⠀⠀⠙⣡⣾⣿⣿⣿⣿⣿⣿⣿⣷⣌⠋⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⣷⣄⡈⢻⣿⡟⢁⣠⣾⣿⣦⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// Для шоу-макета⠀⠀⠀⠙⣿⣿⣿⣿⠘⣿⠃⣿⣿⣿⣿⠋⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// Золотое Кольцо⠀⠀⠀⣀⠀⠈⠛⣰⠿⣆⠛⠁⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀Special⠀⠀⠀⠀⠀⢀⣼⣿⣦⠀⠘⠛⠋⠀⣴⣿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣶⣾⣿⣿⣿⣿⡇⠀⠀⠀⢸⣿⣏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⣠⣶⣿⣿⣿⣿⣿⣿⣿⣿⠿⠿⠀⠀⠀⠾⢿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⣠⣿⣿⣿⣿⣿⣿⡿⠟⠋⣁⣠⣤⣤⡶⠶⠶⣤⣄⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⢰⣿⣿⣮⣉⣉⣉⣤⣴⣶⣿⣿⣋⡥⠄⠀⠀⠀⠀⠉⢻⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⣋⣁⣤⣀⣀⣤⣤⣤⣤⣄⣿⡄⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠙⠿⣿⣿⣿⣿⣿⣿⣿⡿⠿⠛⠋⠉⠁⠀⠀⠀⠀⠈⠛⠃⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀Static Timer⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀v2.0⠀⠀⠀ ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//**************************************************
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//

  ///////////////////////////////////////////////////////////////////////////////////////////////////////*/