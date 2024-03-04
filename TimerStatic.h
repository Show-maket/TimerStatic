class Timer {
private:
  static inline Timer* head = nullptr;
  static inline Timer* last = nullptr;

  typedef void (*CallbackFunc)();
  typedef void (*CallbackFuncParam)(void *);
  typedef unsigned long(*TimeFunc)();
  typedef uint32_t(*LifeShortenerFunc)(Timer* t);

public:
  static void tick() {
    Timer* current = Timer::head;
    while (current != nullptr) {
      current->check();
      current = current->next;
    }
  }

private:
  Timer* next;
  unsigned long nextTimeTrigger, period;
  TimeFunc t_func;
  CallbackFuncParam callback;
  void* obj = nullptr;
  bool isRun = true;
  bool isInf;
  uint32_t life = 0;
  LifeShortenerFunc lifeShortener = Timer::lifeShortenerCount;
  bool setNew = false;

  static inline uint32_t lifeShortenerCount(Timer* timer) {
    return timer->life - 1;
  }
  static inline uint32_t lifeShortenerTime(Timer* timer) {
    return timer->life - (timer->period + ((timer->t_func() - timer->nextTimeTrigger) - timer->period));
  }

  void _Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callback, bool isPre) {
    if (Timer::head == nullptr) {
      Timer::head = this;
    }
    if (last != nullptr) {
      last->next = this;
    }
    last = this;

    this->t_func = t_func;
    this->isInf = true;
    if (!callback || !t_func) {
      isRun = false;
    } else {
      this->nextTimeTrigger = isPre ? t_func() - period : t_func();
    }
    this->period = time;
    this->callback = callback;
  }
public:

  Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callback, bool isPre = false) {
    _Timer(time, t_func, callback, isPre);
  }
  Timer(unsigned long time, TimeFunc t_func, CallbackFunc callback, bool isPre = false) {
    _Timer(time, t_func, (CallbackFuncParam)callback, isPre);
  }
  Timer(void* obj) : obj(obj) {
    _Timer(0, nullptr, nullptr, false);
  }
  Timer(){
    _Timer(0, nullptr, nullptr, false);
  }

  void setObj(void* obj) {
    this->obj = obj;
  }

  void check() {
    if (callback && t_func) {
      uint32_t periodTmp = period;
      setNew = false;
      if (t_func() - nextTimeTrigger >= periodTmp && isRun) {
        // Serial.println((String)life + "   " + (String)periodTmp);
        callback(obj);
        if (setNew) { /* Serial.println("Новый");  */return; }
        uint32_t lifeShortenerVal = lifeShortener(this);
        // Serial.print("--"); Serial.println(lifeShortenerVal);
        if (!this->isInf && this->life < lifeShortenerVal) {
          this->isRun = false;
          // Serial.println("TIMER STOP");
        }

        this->life = lifeShortenerVal;
        do {
          nextTimeTrigger += periodTmp;
          if (nextTimeTrigger < periodTmp) break;          // переполнение uint32_t
        } while (periodTmp != 0 && nextTimeTrigger < t_func() - periodTmp);   // защита от пропуска шага
      }
    }
  }

  void resetToStart() {
    nextTimeTrigger = t_func();
  }
  void resetToEnd() {
    nextTimeTrigger = t_func() - period;
  }

  void ON() {
    this->isRun = true;
  }
  void OFF() {
    this->isRun = false;
  }

  void delay(uint32_t time = 0, TimeFunc t_func = nullptr, CallbackFunc callback = nullptr) {
    delay(time, t_func, (CallbackFuncParam)callback);
  }
  void delay(uint32_t time = 0, TimeFunc t_func = nullptr, CallbackFuncParam callback = nullptr) {
    this->lifeShortener = Timer::lifeShortenerCount;
    this->period = time;
    this->t_func = t_func;
    this->callback = callback;
    this->life = 0;
    this->isRun = true;
    this->isInf = false;
    this->setNew = true;
    this->nextTimeTrigger = t_func();
    // Serial.println("S");
  }

  void forCount(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint16_t lifeCount, bool isPre = true) {
    forCount(time, t_func, (CallbackFuncParam)callback, lifeCount, isPre);
  }
  void forCount(uint32_t time, TimeFunc t_func, CallbackFuncParam callback, uint16_t lifeCount, bool isPre = true) {

    this->lifeShortener = Timer::lifeShortenerCount;
    this->period = time;
    this->t_func = t_func;
    this->callback = callback;
    this->life = lifeCount;
    this->isInf = false;
    this->isRun = lifeCount != 0;
    // this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

  void forTime(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint32_t lifeTime, bool isPre = true) {
    forTime(time, t_func, (CallbackFuncParam)callback, lifeTime, isPre);
  }
  void forTime(uint32_t time, TimeFunc t_func, CallbackFuncParam callback, uint32_t lifeTime, bool isPre = true) {
    this->lifeShortener = Timer::lifeShortenerTime;
    this->period = time;
    this->t_func = t_func;
    this->callback = callback;
    this->life = lifeTime;
    this->isInf = false;
    this->isRun = lifeTime != 0;
    this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

  bool isForLast() {
    return  life < lifeShortener(this);
  }

  void set(unsigned long time, TimeFunc t_func, CallbackFunc callback, bool isPre = false) {
    set(time, t_func, (CallbackFuncParam)callback, isPre);
  }
  void set(unsigned long time, TimeFunc t_func, CallbackFuncParam callback, bool isPre = false) {
    this->lifeShortener = Timer::lifeShortenerCount;
    this->period = time;
    this->t_func = t_func;
    this->callback = callback;
    this->life = 0;
    this->isInf = true;
    this->isRun = true;
    this->setNew = true;
    this->nextTimeTrigger = isPre ? t_func() - period : t_func();
  }

  void setPeriod(uint32_t val) {
    this->period = val;
  }

  void setCallback(CallbackFunc callback) {
    setCallback((CallbackFuncParam)callback);
  }
  void setCallback(CallbackFuncParam callback) {
    this->callback = callback;
  }

};

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