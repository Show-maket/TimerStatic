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
      isRun = false;
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
  Timer::Timer(void *obj) : obj(obj)
  {
    _Timer(0, nullptr, nullptr, false);
  }
  Timer::Timer()
  {
    _Timer(0, nullptr, nullptr, false);
  }
  Timer::Timer(TimeFunc t_func, void *obj): obj(obj){
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
    this->isRun = true;
    this->isInf = false;
    this->setNew = true;
    this->nextTimeTrigger = t_func();

    // Serial.println("S");
  }

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
    this->isRun = lifeCount != 0;
    // this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

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
    this->isRun = lifeTime != 0;
    this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

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
    this->isRun = true;
    this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

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

  Timer::~Timer() {
    // Если список пуст, ничего не делаем
    if (head == nullptr) {
        return;
    }

    // Если удаляемый элемент — голова списка
    if (this == head) {
        head = this->next;  // Перемещаем голову на следующий элемент
        // Если это был единственный элемент, обнуляем last
        if (this == last) {
            last = nullptr;
        }
    } else {
        // Ищем предыдущий элемент перед this
        Timer* prev = head;
        while (prev != nullptr && prev->next != this) {
            prev = prev->next;
        }

        // Если нашли предыдущий элемент, перелинковываем
        if (prev != nullptr) {
            prev->next = this->next;
        }

        // Если удаляемый элемент — последний, обновляем last
        if (this == last) {
            last = prev;
        }
    }

    // Обнуляем указатель next у удаляемого элемента (на всякий случай)
    this->next = nullptr;
}
