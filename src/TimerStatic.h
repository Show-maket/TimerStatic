#pragma once
#include "Arduino.h"

// Diagnostic hook for observing every intrusive-list visit made by tick().
// Keep it completely out of normal builds unless a global compiler flag
// explicitly enables it for every translation unit.
#ifndef TIMER_STATIC_ENABLE_OBSERVER
#define TIMER_STATIC_ENABLE_OBSERVER 0
#endif
#if TIMER_STATIC_ENABLE_OBSERVER != 0 && TIMER_STATIC_ENABLE_OBSERVER != 1
#error "TIMER_STATIC_ENABLE_OBSERVER must be 0 or 1"
#endif

#ifndef __AVR__
#include <functional>
#endif
class Timer
{
private:
  static Timer *head;
  static Timer *last;
  static Timer *tickCursor;
  static Timer *tickCurrent;
  static Timer *tickBoundary;
  static bool inTick;

  typedef void (*CallbackFunc)();
  typedef void (*CallbackFuncParam)(void *);
  typedef unsigned long (*TimeFunc)();
  typedef uint32_t (*LifeShortenerFunc)(Timer *t);
  enum class CallbackKind : uint8_t {
    None,
    Plain,
    Param,
#ifndef __AVR__
    Std,
#endif
  };

  struct CallbackFrame {
    Timer *timer;
    CallbackFrame *previous;
  };

  class CallbackGuard {
  public:
#ifndef __AVR__
    CallbackGuard(Timer *timer, CallbackKind invokedKind,
                  std::function<void()> *detachedStdCallback);
#else
    explicit CallbackGuard(Timer *timer);
#endif
    ~CallbackGuard();

  private:
    CallbackFrame frame;
#ifndef __AVR__
    CallbackKind invokedKind;
    std::function<void()> *detachedStdCallback;
#endif
  };

#if TIMER_STATIC_ENABLE_OBSERVER
public:
  typedef void (*TickObserver)(const Timer *timer);
#endif

public:
  static void tick();
#if TIMER_STATIC_ENABLE_OBSERVER
  static void setTickObserver(TickObserver observer);
#endif

private:
#if TIMER_STATIC_ENABLE_OBSERVER
  static TickObserver tickObserver;
#endif
  static CallbackFrame *callbackFrameTop;
  static uint32_t lifeShortenerCount(Timer *timer);
  static uint32_t lifeShortenerTime(Timer *timer);
  Timer *previous = nullptr;
  Timer *next = nullptr;
  unsigned long nextTimeTrigger = 0;
  unsigned long period = 0;
  TimeFunc t_func = nullptr;
  CallbackFuncParam callbackParam = nullptr;
  CallbackFunc callback = nullptr;
#ifndef __AVR__
  std::function<void()> callbackStdFunc;
#endif
  void *obj = nullptr;
  bool isRun_ = true;
  bool isInf = true;
  uint32_t life = 0;
  LifeShortenerFunc lifeShortener = Timer::lifeShortenerCount;
  bool linked = false;
  bool callbackActive = false;
  bool allowZeroPeriod = false;
  CallbackKind callbackKind = CallbackKind::None;

  void _Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre);
  void unlinkFromList() noexcept;
  void detachLogicalNode() noexcept;
  void adoptMovedState(Timer& other) noexcept;
  bool hasValidCallback() const;
  bool isRunnableConfiguration() const;
  void selectCallback(CallbackFunc callback);
  void selectCallback(CallbackFuncParam callbackP);
#ifndef __AVR__
  void selectCallback(std::function<void()> callbackStd);
#endif

public:
  Timer(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre = false);
  #ifndef __AVR__
  Timer(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre = false);
  #else
  Timer(unsigned long time, TimeFunc t_func, CallbackFunc callbackNoP, bool isPre = false);
  #endif
  Timer(void *obj);
  Timer(TimeFunc t_func, void *obj);
  Timer();

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;
  Timer(Timer&& other) noexcept;
  Timer& operator=(Timer&& other) noexcept;

  ~Timer();

  void check();
  inline void setObj(void *obj) { this->obj = obj; }
  void setTimeFunc(TimeFunc tFunc);
  inline void resetToStart() { if (t_func != nullptr) nextTimeTrigger = t_func() + period; }
  inline void resetToEnd() { if (t_func != nullptr) nextTimeTrigger = t_func(); }

  // Поведенческие гарантии (политики таймера):
  // 1) Защита от джиттера:
  //    - Пропущенные тики НЕ приводят к множественным вызовам коллбэка.
  //    - Внутри check() внутренний nextTimeTrigger «догоняется» до текущего времени,
  //      так что общий график срабатываний не сдвигается, а пропущенные периоды считаются утерянными.
  //    - Коллбэк исполняется максимум один раз за проход check(), даже если было пропущено несколько периодов.
  // 2) forTime(lifeTime): (вариант B — по «таймерному времени»)
  //    - Таймер считает lifeTime в миллисекундах, но списывает его «шагами» периодов (period) и их кратностями,
  //      если tick() был вызван с запозданием (джиттер): пропущенные шаги не исполняются задним числом.
  //    - Перед исполнением callback вычисляется, сколько «таймерного времени» списать за текущий тик.
  //    - Если оставшегося life НЕ хватает на полный шаг, callback ВСЁ РАВНО исполняется как последний:
  //      life принудительно становится 0, isForLast()==true внутри этого callback, и таймер остановится
  //      сразу после callback (если код внутри callback не продлит life через setLifeTime()).

  void setLifeCount(uint16_t newLifeCount);
  void setLifeTime(uint32_t newLifeTime);

  bool resume();
  bool restart();
  inline void ON() { (void)resume(); }
  inline void OFF() { this->isRun_ = false; /* SerialUSB.print("Timer ");SerialUSB.print(period); SerialUSB.println(" OFF"); */}
  inline bool isRun() const { return this->isRun_; }

  // Геттеры для получения оставшегося времени
  unsigned long getRemainingTime() const;
  bool isTimeExpired() const;

  void delay(uint32_t time, TimeFunc t_func, CallbackFunc callback);
  void delay(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP);
#ifndef __AVR__
  void delay_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd);
#endif
  void forCount(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint16_t lifeCount, bool isPre = true);
  void forCount(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint16_t lifeCount, bool isPre = true);
#ifndef __AVR__
  void forCount_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint16_t lifeCount, bool isPre = true);
#endif
  void forTime(uint32_t time, TimeFunc t_func, CallbackFunc callback, uint32_t lifeTime, bool isPre = true);
  void forTime(uint32_t time, TimeFunc t_func, CallbackFuncParam callbackP, uint32_t lifeTime, bool isPre = true);
#ifndef __AVR__
  void forTime_std(uint32_t time, TimeFunc t_func, std::function<void()> callbackStd, uint32_t lifeTime, bool isPre = true);
#endif
  bool isForLast();

  void set(unsigned long time, TimeFunc t_func, CallbackFunc callback, bool isPre = false);
  void set(unsigned long time, TimeFunc t_func, CallbackFuncParam callbackP, bool isPre = false);
#ifndef __AVR__
  void set_std(unsigned long time, TimeFunc t_func, std::function<void()> callbackStd, bool isPre = false);
#endif
  void setPeriod(uint32_t val);
  inline uint32_t getPeriod() { return this->period; }
  inline void restartWithPeriod(uint32_t newPeriod) { setPeriod(newPeriod); resetToStart(); }

  void setCallback(CallbackFunc callback);
  void setCallback(CallbackFuncParam callbackP);
#ifndef __AVR__
  void setCallback_std(std::function<void()> func);
#endif
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
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀v2.4⠀⠀⠀ ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//**************************************************
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//

  ///////////////////////////////////////////////////////////////////////////////////////////////////////*/
