/*
 * РЕАЛИСТИЧНЫЙ полный тест всех случаев использования Timer
 * 
 * Тестируется:
 * 1. Обычная работа таймера
 * 2. resetToStart() внутри callback
 * 3. resetToEnd() внутри callback (как в IR модуле)
 * 4. resetToStart() снаружи (имитация получения данных)
 * 5. resetToEnd() снаружи
 * 6. Защита от джиттера (реальная задержка)
 * 7. Защита от переполнения (millis() + offset)
 * 8. Последовательные вызовы reset
 */

#include <TimerStatic.h>

// Для теста переполнения
uint32_t timeOffset = 0;
uint32_t getTimeWithOffset() {
  return millis() + timeOffset;
}

// =========================================================================
// Базовый класс теста
// =========================================================================
class TestBase {
public:
  int count = 0;
  Timer* timer = nullptr;
  const char* name;
  bool testPassed = false;
  bool wasRun = false;
  
  // Для анализа сетки
  static const int MAX_CALLBACKS = 50;
  uint32_t callbackTimes[MAX_CALLBACKS];
  int callbackCount = 0;
  
  TestBase(const char* testName) : name(testName) {}
  virtual ~TestBase() {}
  
  virtual void run() = 0;
  
  void recordCallback(uint32_t time) {
    if (callbackCount < MAX_CALLBACKS) {
      callbackTimes[callbackCount++] = time;
    }
  }
  
  void analyzeGrid(uint32_t expectedPeriod, bool printDetails = false) {
    if (callbackCount < 2) return;
    
    Serial.println("\n📊 Анализ сетки:");
    
    int consecutiveCalls = 0;
    int maxJitter = 0;
    int totalJitter = 0;
    int jitterCount = 0;
    
    for (int i = 1; i < callbackCount; i++) {
      uint32_t delta = callbackTimes[i] - callbackTimes[i-1];
      int jitter = (int)delta - (int)expectedPeriod;
      
      // Предупреждение о вызовах подряд
      if (delta < 10) {
        consecutiveCalls++;
        Serial.print("⚠️  ВНИМАНИЕ: Вызов #");
        Serial.print(i-1);
        Serial.print(" и #");
        Serial.print(i);
        Serial.print(" почти подряд (");
        Serial.print(delta);
        Serial.println("ms)");
      }
      
      if (printDetails) {
        Serial.print("  Интервал ");
        Serial.print(i-1);
        Serial.print("→");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(delta);
        Serial.print("ms (джиттер: ");
        if (jitter >= 0) Serial.print("+");
        Serial.print(jitter);
        Serial.println("ms)");
      }
      
      if (abs(jitter) > abs(maxJitter)) maxJitter = jitter;
      totalJitter += abs(jitter);
      jitterCount++;
    }
    
    Serial.print("  Средний джиттер: ±");
    Serial.print(jitterCount > 0 ? totalJitter / jitterCount : 0);
    Serial.print("ms, макс: ");
    if (maxJitter >= 0) Serial.print("+");
    Serial.print(maxJitter);
    Serial.println("ms");
    
    if (consecutiveCalls > 0) {
      Serial.print("  ⚠️  Обнаружено вызовов подряд: ");
      Serial.println(consecutiveCalls);
    } else {
      Serial.println("  ✓ Вызовов подряд не обнаружено");
    }
  }
  
  void passed(const char* msg = nullptr) {
    if (msg) Serial.println(msg);
    Serial.println("✓ PASSED\n");
    testPassed = true;
    wasRun = true;
    delay(500);
  }
  
  void failed(const char* msg = nullptr) {
    if (msg) Serial.println(msg);
    Serial.println("✗ FAILED\n");
    testPassed = false;
    wasRun = true;
    delay(500);
  }
};

// =========================================================================
// ТЕСТ 1: Обычная работа
// =========================================================================
class Test1 : public TestBase {
public:
  Timer myTimer;
  static const uint32_t PERIOD = 300;
  static const int EXPECTED_COUNT = 10;
  
  Test1() : TestBase("TEST 1: Обычная работа (период=300ms, итераций=10)"), 
            myTimer(PERIOD, millis, [](void* obj) {
              Test1* self = (Test1*)obj;
              uint32_t now = millis();
              self->count++;
              self->recordCallback(now);
              Serial.print("  [Test1] Callback #");
              Serial.print(self->count);
              Serial.print(" at ");
              Serial.println(now);
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 1: Обычная работа таймера ==========");
    Serial.print("📋 Параметры: период=");
    Serial.print(PERIOD);
    Serial.print("ms, ожидается ");
    Serial.print(EXPECTED_COUNT);
    Serial.println(" срабатываний\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    uint32_t timeout = PERIOD * EXPECTED_COUNT + 1000; // +1с запас
    
    while (count < EXPECTED_COUNT && millis() - start < timeout) {
      myTimer.check();
      
    }
    
    Serial.print("\nПолучено вызовов: ");
    Serial.println(count);
    
    // Анализ сетки
    analyzeGrid(PERIOD);
    
    count == EXPECTED_COUNT ? passed() : failed();
  }
};

// =========================================================================
// ТЕСТ 2: resetToStart() ВНУТРИ callback
// =========================================================================
class Test2 : public TestBase {
public:
  Timer myTimer;
  static const uint32_t PERIOD = 300;
  static const int EXPECTED_COUNT = 8;
  uint32_t resetTime = 0;
  
  Test2() : TestBase("TEST 2: resetToStart() внутри callback (период=300ms, reset на 3-м)"),
            myTimer(PERIOD, millis, [](void* obj) {
              Test2* self = (Test2*)obj;
              uint32_t now = millis();
              self->count++;
              self->recordCallback(now);
              Serial.print("  [Test2] Callback #");
              Serial.print(self->count);
              Serial.print(" at ");
              Serial.println(now);
              
              if (self->count == 3) {
                Serial.println("  → resetToStart() вызван в callback #3");
                self->resetTime = now;
                self->myTimer.resetToStart();
              }
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 2: resetToStart() внутри callback ==========");
    Serial.print("📋 Параметры: период=");
    Serial.print(PERIOD);
    Serial.print("ms, reset на 3-м вызове, всего ");
    Serial.print(EXPECTED_COUNT);
    Serial.println(" итераций\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    uint32_t timeout = PERIOD * EXPECTED_COUNT + 1000;
    
    while (count < EXPECTED_COUNT && millis() - start < timeout) {
      myTimer.check();
      
    }
    
    Serial.print("\nПолучено вызовов: ");
    Serial.println(count);
    
    // Анализ сетки с учетом reset
    Serial.println("\n📊 Анализ сетки до reset:");
    analyzeGrid(PERIOD, false);
    
    if (resetTime > 0 && callbackCount > 3) {
      Serial.println("\n🔄 Смещение сетки после reset:");
      Serial.print("  Reset произошел в: ");
      Serial.print(resetTime);
      Serial.print("ms, следующий вызов ожидался в: ");
      Serial.println(resetTime + PERIOD);
    }
    
    count >= EXPECTED_COUNT ? passed() : failed();
  }
};

// =========================================================================
// ТЕСТ 3: resetToEnd() ВНУТРИ callback (как в IR модуле)
// =========================================================================
class Test3 : public TestBase {
public:
  Timer myTimer;
  static const uint32_t PERIOD = 400;
  static const int EXPECTED_COUNT = 8;
  unsigned long time3 = 0, time4 = 0;
  
  Test3() : TestBase("TEST 3: resetToEnd() внутри callback (период=400ms, немедленный повтор)"),
            myTimer(PERIOD, millis, [](void* obj) {
              Test3* self = (Test3*)obj;
              uint32_t now = millis();
              self->count++;
              self->recordCallback(now);
              Serial.print("  [Test3] Callback #");
              Serial.print(self->count);
              Serial.print(" at ");
              Serial.println(now);
              
              if (self->count == 3) {
                Serial.println("  → resetToEnd() вызван (имитация SKIP LASER)");
                self->time3 = now;
                self->myTimer.resetToEnd();
              }
              if (self->count == 4) {
                self->time4 = now;
              }
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 3: resetToEnd() внутри callback ==========");
    Serial.print("📋 Параметры: период=");
    Serial.print(PERIOD);
    Serial.print("ms, resetToEnd на 3-м вызове (ожидается немедленный повтор), всего ");
    Serial.print(EXPECTED_COUNT);
    Serial.println(" итераций\n");
    
    count = 0;
    time3 = 0;
    time4 = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    uint32_t timeout = PERIOD * EXPECTED_COUNT + 1000;
    
    while (count < EXPECTED_COUNT && millis() - start < timeout) {
      myTimer.check();
      
    }
    
    Serial.print("\nВремя между callback #3 и #4: ");
    Serial.print(time4 - time3);
    Serial.println("ms");
    
    // Анализ сетки
    analyzeGrid(PERIOD);
    
    (time4 - time3) < 50 ? passed("Немедленное срабатывание!") : failed("Слишком долго");
  }
};

// =========================================================================
// ТЕСТ 4: resetToStart() СНАРУЖИ (имитация получения IR данных)
// =========================================================================
class Test4 : public TestBase {
public:
  Timer myTimer;
  
  Test4() : TestBase("TEST 4: resetToStart() снаружи"),
            myTimer(350, millis, [](void* obj) {
              Test4* self = (Test4*)obj;
              self->count++;
              Serial.print("  [Test4] Callback #");
              Serial.print(self->count);
              Serial.print(" at ");
              Serial.println(millis());
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 4: resetToStart() снаружи callback ==========");
    Serial.println("Имитация IR: получены данные -> resetToStart()\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool resetDone = false;
    
    while (millis() - start < 2000) {
      myTimer.check();
      
      if (millis() - start > 600 && !resetDone) {
        Serial.println("\n  → Получены IR данные! resetToStart()");
        myTimer.resetToStart();
        resetDone = true;
      }
      
      
    }
    
    Serial.print("\nПолучено вызовов: ");
    Serial.println(count);
    count >= 3 ? passed() : failed();
  }
};

// =========================================================================
// ТЕСТ 5: resetToEnd() СНАРУЖИ
// =========================================================================
class Test5 : public TestBase {
public:
  Timer myTimer;
  
  Test5() : TestBase("TEST 5: resetToEnd() снаружи"),
            myTimer(400, millis, [](void* obj) {
              Test5* self = (Test5*)obj;
              self->count++;
              Serial.print("  [Test5] Callback #");
              Serial.print(self->count);
              Serial.print(" at ");
              Serial.println(millis());
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 5: resetToEnd() снаружи callback ==========");
    Serial.println("Вызов resetToEnd() между тиками\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    int countBefore = 0;
    bool resetDone = false;
    
    while (millis() - start < 1500) {
      myTimer.check();
      
      if (millis() - start > 700 && !resetDone) {
        countBefore = count;
        Serial.println("\n  → resetToEnd() снаружи");
        myTimer.resetToEnd();
        resetDone = true;
      }
      
      
    }
    
    Serial.print("\nВызовов до reset: ");
    Serial.print(countBefore);
    Serial.print(", после: ");
    Serial.println(count - countBefore);
    count > countBefore ? passed("Немедленное срабатывание!") : failed();
  }
};

// =========================================================================
// ТЕСТ 6: Защита от джиттера
// =========================================================================
class Test6 : public TestBase {
public:
  Timer myTimer;
  static const uint32_t PERIOD = 200;
  static const uint32_t DELAY_MS = 1000;
  static const int EXPECTED_COUNT = 12;
  uint32_t delayStart = 0;
  uint32_t delayEnd = 0;
  
  Test6() : TestBase("TEST 6: Защита от джиттера (период=200ms, задержка=1000ms)"),
            myTimer(PERIOD, millis, [](void* obj) {
              Test6* self = (Test6*)obj;
              uint32_t now = millis();
              self->count++;
              self->recordCallback(now);
              Serial.print("  [Test6] Callback #");
              Serial.print(self->count);
              Serial.print(" at ");
              Serial.println(now);
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 6: Защита от джиттера ==========");
    Serial.print("📋 Параметры: период=");
    Serial.print(PERIOD);
    Serial.print("ms, задержка=");
    Serial.print(DELAY_MS);
    Serial.print("ms после 2-го вызова, всего ");
    Serial.print(EXPECTED_COUNT);
    Serial.println(" итераций\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool delayDone = false;
    uint32_t timeout = PERIOD * EXPECTED_COUNT + DELAY_MS + 500;
    
    while (count < EXPECTED_COUNT && millis() - start < timeout) {
      myTimer.check();
      
      if (count == 2 && !delayDone) {
        delayStart = millis();
        Serial.print("\n  → ЗАДЕРЖКА ");
        Serial.print(DELAY_MS);
        Serial.println("ms (зависание)");
        delay(DELAY_MS);
        delayEnd = millis();
        Serial.println("  → Продолжаем\n");
        delayDone = true;
      }
      
      
    }
    
    Serial.print("\nПолучено вызовов: ");
    Serial.println(count);
    
    // Детальный анализ джиттера
    Serial.println("\n📊 Анализ поведения при джиттере:");
    Serial.print("  Реальная задержка: ");
    Serial.print(delayEnd - delayStart);
    Serial.println("ms");
    
    // Проверка сколько периодов пропущено
    int missedPeriods = DELAY_MS / PERIOD;
    Serial.print("  Пропущено периодов: ");
    Serial.println(missedPeriods);
    Serial.print("  Коллбэки не доисполнялись пачкой: ");
    
    // Проверка что после задержки не было пачки вызовов
    bool noBurst = true;
    for (int i = 3; i < callbackCount && i < 6; i++) {
      if (callbackTimes[i] - callbackTimes[i-1] < 50) {
        noBurst = false;
        break;
      }
    }
    Serial.println(noBurst ? "✓" : "✗");
    
    // Полный анализ сетки
    analyzeGrid(PERIOD, true);
    
    count >= EXPECTED_COUNT && noBurst ? passed("Джиттер обработан корректно!") : failed();
  }
};

// =========================================================================
// ТЕСТ 7: Защита от переполнения
// =========================================================================
class Test7 : public TestBase {
public:
  Timer myTimer;
  
  Test7() : TestBase("TEST 7: Защита от переполнения"),
            myTimer(300, getTimeWithOffset, [](void* obj) {
              Test7* self = (Test7*)obj;
              self->count++;
              Serial.print("  [Test7] Callback #");
              Serial.print(self->count);
              Serial.print(" at offset_time=");
              Serial.println(getTimeWithOffset());
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 7: Защита от переполнения ==========");
    Serial.println("millis() + offset близко к UINT32_MAX\n");
    
    timeOffset = 4294967000UL - millis();
    Serial.print("Начальное время: ");
    Serial.println(getTimeWithOffset());
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    
    while (count < 5 && millis() - start < 2000) {
      myTimer.check();
      
    }
    
    Serial.print("\nПолучено вызовов: ");
    Serial.println(count);
    
    timeOffset = 0;
    count >= 3 ? passed("Переполнение обработано!") : failed();
  }
};

// =========================================================================
// ТЕСТ 8: Последовательные reset вызовы
// =========================================================================
class Test8 : public TestBase {
public:
  Timer myTimer;
  unsigned long time2 = 0, time3 = 0;
  
  Test8() : TestBase("TEST 8: Последовательные reset"),
            myTimer(350, millis, [](void* obj) {
              Test8* self = (Test8*)obj;
              self->count++;
              Serial.print("  [Test8] Callback #");
              Serial.print(self->count);
              Serial.print(" at ");
              Serial.println(millis());
              
              if (self->count == 2) {
                Serial.println("  → resetToEnd() затем resetToStart()");
                self->time2 = millis();
                self->myTimer.resetToEnd();
                self->myTimer.resetToStart(); // Последний должен победить
              }
              if (self->count == 3) {
                self->time3 = millis();
              }
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 8: Последовательные reset ==========");
    Serial.println("resetToEnd() затем resetToStart()\n");
    
    count = 0;
    time2 = 0;
    time3 = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    
    while (count < 5 && millis() - start < 2500) {
      myTimer.check();
      
    }
    
    Serial.print("\nВремя между #2 и #3: ");
    Serial.print(time3 - time2);
    Serial.println("ms");
    (time3 - time2) > 200 ? passed("resetToStart победил!") : failed();
  }
};

// =========================================================================
// ТЕСТ 9: ON() / OFF() - включение/выключение таймера
// =========================================================================
class Test9 : public TestBase {
public:
  Timer myTimer;
  
  Test9() : TestBase("TEST 9: ON/OFF управление"),
            myTimer(200, millis, [](void* obj) {
              Test9* self = (Test9*)obj;
              self->count++;
              Serial.print("  [Test9] Callback #");
              Serial.println(self->count);
            }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 9: ON() / OFF() управление ==========");
    Serial.println("Таймер выключается после 2-го срабатывания\n");
    
    count = 0;
    myTimer.resetToStart();
    myTimer.ON();
    unsigned long start = millis();
    bool offCalled = false;
    
    while (millis() - start < 1500) {
      myTimer.check();
      
      if (count == 2 && !offCalled) {
        Serial.println("  → OFF() вызван");
        myTimer.OFF();
        offCalled = true;
      }
      
      
    }
    
    Serial.print("\nПолучено вызовов: ");
    Serial.println(count);
    Serial.print("isRun: ");
    Serial.println(myTimer.isRun() ? "true" : "false");
    (count == 2 && !myTimer.isRun()) ? passed("Таймер остановлен!") : failed();
  }
};

// =========================================================================
// ТЕСТ 10: isRun() - проверка состояния таймера
// =========================================================================
class Test10 : public TestBase {
public:
  Timer myTimer;
  
  Test10() : TestBase("TEST 10: isRun() проверка"),
             myTimer(300, millis, [](void* obj) {
               Test10* self = (Test10*)obj;
               self->count++;
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 10: isRun() проверка состояния ==========");
    Serial.println("Проверяем состояние до и после OFF/ON\n");
    
    myTimer.ON();
    bool state1 = myTimer.isRun();
    Serial.print("После ON(): ");
    Serial.println(state1 ? "true" : "false");
    
    myTimer.OFF();
    bool state2 = myTimer.isRun();
    Serial.print("После OFF(): ");
    Serial.println(state2 ? "false" : "true (ошибка!)");
    
    myTimer.ON();
    bool state3 = myTimer.isRun();
    Serial.print("После повторного ON(): ");
    Serial.println(state3 ? "true" : "false");
    
    (state1 && !state2 && state3) ? passed() : failed("isRun() работает неправильно");
  }
};

// =========================================================================
// ТЕСТ 11: OFF → resetToStart → ON - взаимодействие
// =========================================================================
class Test11 : public TestBase {
public:
  Timer myTimer;
  
  Test11() : TestBase("TEST 11: OFF/reset/ON взаимодействие"),
             myTimer(250, millis, [](void* obj) {
               Test11* self = (Test11*)obj;
               self->count++;
               Serial.print("  [Test11] Callback #");
               Serial.println(self->count);
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 11: OFF → reset → ON ==========");
    Serial.println("Выключаем, сбрасываем, включаем снова\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool resetDone = false;
    
    while (millis() - start < 1200) {
      myTimer.check();
      
      if (millis() - start > 600 && count == 2 && !resetDone) {
        Serial.println("\n  → OFF() + resetToStart() + ON()");
        myTimer.OFF();
        delay(10);
        myTimer.resetToStart();
        myTimer.ON();
        resetDone = true;
      }
      
      
    }
    
    Serial.print("\nПолучено вызовов: ");
    Serial.println(count);
    count >= 3 ? passed("Перезапуск успешен!") : failed();
  }
};

// =========================================================================
// ТЕСТ 12: OFF во время работы + подсчет пропущенных
// =========================================================================
class Test12 : public TestBase {
public:
  Timer myTimer;
  unsigned long offTime = 0, onTime = 0;
  
  Test12() : TestBase("TEST 12: OFF с паузой"),
             myTimer(150, millis, [](void* obj) {
               Test12* self = (Test12*)obj;
               self->count++;
               Serial.print("  [Test12] Callback #");
               Serial.print(self->count);
               Serial.print(" at ");
               Serial.println(millis());
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 12: OFF во время работы ==========");
    Serial.println("Таймер паузится на 500ms\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool paused = false;
    
    while (millis() - start < 1500) {
      myTimer.check();
      
      if (millis() - start > 400 && !paused && count >= 2) {
        Serial.println("\n  → Пауза 500ms (OFF)");
        offTime = millis();
        myTimer.OFF();
        delay(500);
        myTimer.ON();
        onTime = millis();
        Serial.println("  → Возобновление (ON)\n");
        paused = true;
      }
      
      
    }
    
    Serial.print("\nВсего вызовов: ");
    Serial.println(count);
    Serial.print("Пауза была: ");
    Serial.print(onTime - offTime);
    Serial.println("ms");
    count >= 4 ? passed("Пауза не пропустила вызовы!") : failed();
  }
};

// =========================================================================
// ТЕСТ 13: setPeriod() - динамическое изменение периода
// =========================================================================
class Test13 : public TestBase {
public:
  Timer myTimer;
  
  Test13() : TestBase("TEST 13: setPeriod()"),
             myTimer(300, millis, [](void* obj) {
               Test13* self = (Test13*)obj;
               self->count++;
               Serial.print("  [Test13] Callback #");
               Serial.print(self->count);
               Serial.print(" at ");
               Serial.println(millis());
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 13: setPeriod() - изменение периода ==========");
    Serial.println("Начало: 300ms, после 2-го: 100ms\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool changed = false;
    
    while (millis() - start < 1500) {
      myTimer.check();
      
      if (count == 2 && !changed) {
        Serial.println("\n  → setPeriod(100) вызван");
        myTimer.setPeriod(100);
        changed = true;
      }
      
      
    }
    
    Serial.print("\nВсего вызовов: ");
    Serial.println(count);
    count >= 6 ? passed("Период изменен!") : failed();
  }
};

// =========================================================================
// ТЕСТ 14: getPeriod() - получение текущего периода
// =========================================================================
class Test14 : public TestBase {
public:
  Timer myTimer;
  
  Test14() : TestBase("TEST 14: getPeriod()"),
             myTimer(250, millis, [](void* obj) {}) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 14: getPeriod() - чтение периода ==========");
    
    uint32_t period1 = myTimer.getPeriod();
    Serial.print("Начальный период: ");
    Serial.println(period1);
    
    myTimer.setPeriod(500);
    uint32_t period2 = myTimer.getPeriod();
    Serial.print("После setPeriod(500): ");
    Serial.println(period2);
    
    myTimer.setPeriod(100);
    uint32_t period3 = myTimer.getPeriod();
    Serial.print("После setPeriod(100): ");
    Serial.println(period3);
    
    (period1 == 250 && period2 == 500 && period3 == 100) ? 
      passed() : failed("getPeriod() неверные значения");
  }
};

// =========================================================================
// ТЕСТ 15: restartWithPeriod() - перезапуск с новым периодом
// =========================================================================
class Test15 : public TestBase {
public:
  Timer myTimer;
  unsigned long firstTime = 0, secondTime = 0;
  
  Test15() : TestBase("TEST 15: restartWithPeriod()"),
             myTimer(400, millis, [](void* obj) {
               Test15* self = (Test15*)obj;
               self->count++;
               Serial.print("  [Test15] Callback #");
               Serial.print(self->count);
               Serial.print(" at ");
               Serial.println(millis());
               
               if (self->count == 1) self->firstTime = millis();
               if (self->count == 2) self->secondTime = millis();
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 15: restartWithPeriod() ==========");
    Serial.println("Начало: 400ms, после 1-го: restartWithPeriod(150)\n");
    
    count = 0;
    firstTime = 0;
    secondTime = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool restarted = false;
    
    while (count < 3 && millis() - start < 2000) {
      myTimer.check();
      
      if (count == 1 && !restarted) {
        Serial.println("\n  → restartWithPeriod(150)");
        myTimer.restartWithPeriod(150);
        restarted = true;
      }
      
      
    }
    
    uint32_t delta = secondTime - firstTime;
    Serial.print("\nВремя между #1 и #2: ");
    Serial.print(delta);
    Serial.println("ms");
    (delta >= 140 && delta <= 170) ? 
      passed("Период изменен и перезапущен!") : failed("Неверный интервал");
  }
};

// =========================================================================
// ТЕСТ 16: delay() - одноразовое срабатывание
// =========================================================================
class Test16 : public TestBase {
public:
  Timer myTimer;
  unsigned long triggerTime = 0;
  
  Test16() : TestBase("TEST 16: delay()"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 16: delay() - одноразовый таймер ==========");
    Serial.println("Задержка 500ms, должно сработать 1 раз\n");
    
    count = 0;
    triggerTime = 0;
    unsigned long start = millis();
    
    // Настраиваем delay
    myTimer.delay(500, millis, [](void* obj) {
      Test16* self = (Test16*)obj;
      self->count++;
      self->triggerTime = millis();
      Serial.print("  [Test16] Callback! at ");
      Serial.println(self->triggerTime);
    });
    
    while (millis() - start < 1200) {
      myTimer.check();
      
    }
    
    Serial.print("\nВызовов: ");
    Serial.println(count);
    Serial.print("Время срабатывания: ~");
    Serial.print(triggerTime - start);
    Serial.println("ms");
    
    (count == 1 && (triggerTime - start) >= 490 && (triggerTime - start) <= 520) ? 
      passed("Одноразовый!") : failed();
  }
};

// =========================================================================
// ТЕСТ 17: delay() с последовательными вызовами
// =========================================================================
class Test17 : public TestBase {
public:
  Timer myTimer;
  int phase = 0;
  unsigned long time1 = 0, time2 = 0, time3 = 0;
  
  Test17() : TestBase("TEST 17: delay() последовательные"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 17: delay() последовательно ==========");
    Serial.println("Три delay() подряд: 300ms → 200ms → 400ms\n");
    
    count = 0;
    phase = 1;
    unsigned long start = millis();
    
    // Первый delay
    myTimer.delay(300, millis, [](void* obj) {
      Test17* self = (Test17*)obj;
      self->count++;
      self->time1 = millis();
      Serial.println("  [Test17] Фаза 1 завершена (300ms)");
      
      // Запускаем второй delay
      self->phase = 2;
      self->myTimer.delay(200, millis, [](void* obj2) {
        Test17* self2 = (Test17*)obj2;
        self2->count++;
        self2->time2 = millis();
        Serial.println("  [Test17] Фаза 2 завершена (200ms)");
        
        // Запускаем третий delay
        self2->phase = 3;
        self2->myTimer.delay(400, millis, [](void* obj3) {
          Test17* self3 = (Test17*)obj3;
          self3->count++;
          self3->time3 = millis();
          Serial.println("  [Test17] Фаза 3 завершена (400ms)");
          self3->phase = 4;
        });
      });
    });
    
    while (phase < 4 && millis() - start < 2000) {
      myTimer.check();
      
    }
    
    Serial.print("\nВсего вызовов: ");
    Serial.println(count);
    Serial.print("Фаза 1→2: ");
    Serial.print(time2 - time1);
    Serial.print("ms, Фаза 2→3: ");
    Serial.print(time3 - time2);
    Serial.println("ms");
    
    (count == 3 && phase == 4) ? 
      passed("Последовательные delay() работают!") : failed();
  }
};

// =========================================================================
// ТЕСТ 18: forCount() - ограничение по количеству
// =========================================================================
class Test18 : public TestBase {
public:
  Timer myTimer;
  static const uint32_t PERIOD = 200;
  static const int LIMIT = 10;
  
  Test18() : TestBase("TEST 18: forCount() (период=200ms, лимит=10, isPre=true)"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 18: forCount() - ограничение ==========");
    Serial.print("📋 Параметры: период=");
    Serial.print(PERIOD);
    Serial.print("ms, лимит=");
    Serial.print(LIMIT);
    Serial.println(" раз, isPre=true (сразу)\n");
    
    count = 0;
    myTimer.forCount(PERIOD, millis, [](void* obj) {
      Test18* self = (Test18*)obj;
      uint32_t now = millis();
      self->count++;
      self->recordCallback(now);
      Serial.print("  [Test18] Callback #");
      Serial.println(self->count);
    }, LIMIT, true);
    
    unsigned long start = millis();
    uint32_t timeout = PERIOD * (LIMIT + 2); // +2 периода запас
    while (millis() - start < timeout) {
      myTimer.check();
      
    }
    
    Serial.print("\nВсего вызовов: ");
    Serial.println(count);
    Serial.print("isRun: ");
    Serial.println(myTimer.isRun() ? "true (ошибка!)" : "false");
    
    // Анализ сетки
    analyzeGrid(PERIOD);
    
    (count == LIMIT && !myTimer.isRun()) ? 
      passed("Остановлен точно после лимита!") : failed();
  }
};

// =========================================================================
// ТЕСТ 19: forCount() с isPre=false
// =========================================================================
class Test19 : public TestBase {
public:
  Timer myTimer;
  unsigned long firstTime = 0;
  
  Test19() : TestBase("TEST 19: forCount() isPre=false"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 19: forCount() isPre=false ==========");
    Serial.println("Первый вызов НЕ сразу, а через период\n");
    
    count = 0;
    unsigned long start = millis();
    
    myTimer.forCount(300, millis, [](void* obj) {
      Test19* self = (Test19*)obj;
      self->count++;
      if (self->count == 1) self->firstTime = millis();
      Serial.print("  [Test19] Callback #");
      Serial.println(self->count);
    }, 3, false);  // isPre = false
    
    while (count < 3 && millis() - start < 1500) {
      myTimer.check();
      
    }
    
    uint32_t delta = firstTime - start;
    Serial.print("\nВремя до 1-го вызова: ");
    Serial.print(delta);
    Serial.println("ms");
    
    (delta >= 280 && delta <= 320) ? 
      passed("Первый вызов через период!") : failed("Неверная задержка");
  }
};

// =========================================================================
// ТЕСТ 20: forTime() - ограничение по времени
// =========================================================================
class Test20 : public TestBase {
public:
  Timer myTimer;
  
  Test20() : TestBase("TEST 20: forTime()"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 20: forTime() - ограничение ==========");
    Serial.println("Период 150ms, время жизни 600ms\n");
    
    count = 0;
    unsigned long start = millis();
    
    myTimer.forTime(150, millis, [](void* obj) {
      Test20* self = (Test20*)obj;
      self->count++;
      Serial.print("  [Test20] Callback #");
      Serial.print(self->count);
      Serial.print(" at ");
      Serial.println(millis());
    }, 600, true);
    
    while (millis() - start < 1200) {
      myTimer.check();
      
    }
    
    Serial.print("\nВсего вызовов: ");
    Serial.println(count);
    Serial.print("isRun: ");
    Serial.println(myTimer.isRun() ? "true (ошибка!)" : "false");
    
    (count >= 3 && count <= 5 && !myTimer.isRun()) ? 
      passed("Остановлен по времени!") : failed();
  }
};

// =========================================================================
// ТЕСТ 21: forTime() с isPre=false
// =========================================================================
class Test21 : public TestBase {
public:
  Timer myTimer;
  unsigned long firstTime = 0;
  
  Test21() : TestBase("TEST 21: forTime() isPre=false"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 21: forTime() isPre=false ==========");
    Serial.println("Период 250ms, жизнь 800ms, isPre=false\n");
    
    count = 0;
    unsigned long start = millis();
    
    myTimer.forTime(250, millis, [](void* obj) {
      Test21* self = (Test21*)obj;
      self->count++;
      if (self->count == 1) self->firstTime = millis();
      Serial.print("  [Test21] Callback #");
      Serial.println(self->count);
    }, 800, false);
    
    while (millis() - start < 1500) {
      myTimer.check();
      
    }
    
    uint32_t delta = firstTime - start;
    Serial.print("\nВремя до 1-го: ");
    Serial.print(delta);
    Serial.print("ms, всего вызовов: ");
    Serial.println(count);
    
    (delta >= 230 && delta <= 270 && count >= 2) ? 
      passed() : failed();
  }
};

// =========================================================================
// ТЕСТ 22: isForLast() - определение последней итерации
// =========================================================================
class Test22 : public TestBase {
public:
  Timer myTimer;
  bool lastDetected = false;
  
  Test22() : TestBase("TEST 22: isForLast()"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 22: isForLast() ==========");
    Serial.println("Определяем последнюю итерацию forCount\n");
    
    count = 0;
    lastDetected = false;
    
    myTimer.forCount(180, millis, [](void* obj) {
      Test22* self = (Test22*)obj;
      self->count++;
      Serial.print("  [Test22] Callback #");
      Serial.print(self->count);
      
      if (self->myTimer.isForLast()) {
        Serial.println(" ← ПОСЛЕДНИЙ!");
        self->lastDetected = true;
      } else {
        Serial.println("");
      }
    }, 4, true);
    
    unsigned long start = millis();
    while (count < 4 && millis() - start < 1200) {
      myTimer.check();
      
    }
    
    Serial.print("\nПоследний обнаружен: ");
    Serial.println(lastDetected ? "Да" : "Нет");
    
    (lastDetected && count == 4) ? 
      passed("isForLast() работает!") : failed();
  }
};

// =========================================================================
// ТЕСТ 23: setLifeCount() - динамическое изменение
// =========================================================================
class Test23 : public TestBase {
public:
  Timer myTimer;
  
  Test23() : TestBase("TEST 23: setLifeCount()"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 23: setLifeCount() ==========");
    Serial.println("Начало: 3 раза, после 2-го: +5 раз\n");
    
    count = 0;
    myTimer.forCount(150, millis, [](void* obj) {
      Test23* self = (Test23*)obj;
      self->count++;
      Serial.print("  [Test23] Callback #");
      Serial.println(self->count);
      
      if (self->count == 2) {
        Serial.println("  → setLifeCount(5) добавляем жизнь!");
        self->myTimer.setLifeCount(5);
      }
    }, 3, true);
    
    unsigned long start = millis();
    while (millis() - start < 1500) {
      myTimer.check();
      
    }
    
    Serial.print("\nВсего вызовов: ");
    Serial.println(count);
    
    (count >= 6 && count <= 8) ? 
      passed("Жизнь продлена!") : failed();
  }
};

// =========================================================================
// ТЕСТ 24: set() - динамическая переконфигурация
// =========================================================================
class Test24 : public TestBase {
public:
  Timer myTimer;
  
  Test24() : TestBase("TEST 24: set()"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 24: set() - переконфигурация ==========");
    Serial.println("Запуск с новыми параметрами на лету\n");
    
    count = 0;
    myTimer.set(250, millis, [](void* obj) {
      Test24* self = (Test24*)obj;
      self->count++;
      Serial.print("  [Test24] Callback #");
      Serial.println(self->count);
    }, false);
    
    unsigned long start = millis();
    while (millis() - start < 1200) {
      myTimer.check();
      
    }
    
    Serial.print("\nВызовов: ");
    Serial.println(count);
    (count >= 4) ? passed("set() работает!") : failed();
  }
};

// =========================================================================
// ТЕСТ 25: setCallback() - смена callback на лету
// =========================================================================
class Test25 : public TestBase {
public:
  Timer myTimer;
  int count2 = 0;
  
  Test25() : TestBase("TEST 25: setCallback()"),
             myTimer(200, millis, [](void* obj) {
               Test25* self = (Test25*)obj;
               self->count++;
               Serial.println("  [Test25] Callback A");
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 25: setCallback() ==========");
    Serial.println("Смена callback после 2-го вызова\n");
    
    count = 0;
    count2 = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool changed = false;
    
    while (millis() - start < 1200) {
      myTimer.check();
      
      if (count == 2 && !changed) {
        Serial.println("  → Меняем callback!");
        myTimer.setCallback([](void* obj) {
          Test25* self = (Test25*)obj;
          self->count2++;
          Serial.println("  [Test25] Callback B");
        });
        changed = true;
      }
      
      
    }
    
    Serial.print("\nCallback A: ");
    Serial.print(count);
    Serial.print(", Callback B: ");
    Serial.println(count2);
    (count == 2 && count2 >= 2) ? passed("Callback изменен!") : failed();
  }
};

// =========================================================================
// ТЕСТ 26: setTimeFunc() - смена функции времени
// =========================================================================
class Test26 : public TestBase {
public:
  Timer myTimer;
  static uint32_t customTime;
  
  static uint32_t getCustomTime() { return customTime; }
  
  Test26() : TestBase("TEST 26: setTimeFunc()"),
             myTimer(100, millis, [](void* obj) {
               Test26* self = (Test26*)obj;
               self->count++;
               Serial.println("  [Test26] Callback");
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 26: setTimeFunc() ==========");
    Serial.println("Переключение с millis() на custom\n");
    
    count = 0;
    customTime = 1000;
    
    // Начинаем с millis
    myTimer.resetToStart();
    unsigned long start = millis();
    
    while (count < 2 && millis() - start < 500) {
      myTimer.check();
      
    }
    
    Serial.println("  → Переключаемся на customTime");
    myTimer.setTimeFunc(getCustomTime);
    myTimer.resetToStart();
    
    // Имитируем работу с custom time
    for (int i = 0; i < 5; i++) {
      customTime += 100;
      myTimer.check();
    }
    
    Serial.print("\nВызовов: ");
    Serial.println(count);
    (count >= 3) ? passed("TimeFunc изменен!") : failed();
  }
};
uint32_t Test26::customTime = 0;

// =========================================================================
// ТЕСТ 27: getRemainingTime() - оставшееся время
// =========================================================================
class Test27 : public TestBase {
public:
  Timer myTimer;
  
  Test27() : TestBase("TEST 27: getRemainingTime()"),
             myTimer(500, millis, [](void* obj) {}) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 27: getRemainingTime() ==========");
    
    myTimer.resetToStart();
    delay(100);
    
    unsigned long remaining1 = myTimer.getRemainingTime();
    Serial.print("После 100ms: осталось ~");
    Serial.println(remaining1);
    
    delay(200);
    unsigned long remaining2 = myTimer.getRemainingTime();
    Serial.print("После ещё 200ms: осталось ~");
    Serial.println(remaining2);
    
    (remaining1 > remaining2 && remaining2 < remaining1) ? 
      passed("Время уменьшается!") : failed();
  }
};

// =========================================================================
// ТЕСТ 28: isTimeExpired() - проверка истечения
// =========================================================================
class Test28 : public TestBase {
public:
  Timer myTimer;
  
  Test28() : TestBase("TEST 28: isTimeExpired()"),
             myTimer(300, millis, [](void* obj) {}) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 28: isTimeExpired() ==========");
    
    myTimer.resetToStart();
    delay(50);
    
    bool expired1 = myTimer.isTimeExpired();
    Serial.print("После 50ms: ");
    Serial.println(expired1 ? "истёк (ошибка!)" : "НЕ истёк");
    
    delay(300);
    bool expired2 = myTimer.isTimeExpired();
    Serial.print("После 350ms: ");
    Serial.println(expired2 ? "истёк" : "НЕ истёк (ошибка!)");
    
    (!expired1 && expired2) ? passed() : failed();
  }
};

// =========================================================================
// ТЕСТ 29: isPre=true - немедленный запуск
// =========================================================================
class Test29 : public TestBase {
public:
  Timer myTimer;
  unsigned long firstTime = 0;
  
  Test29() : TestBase("TEST 29: isPre=true"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 29: isPre=true - немедленно ==========");
    
    count = 0;
    unsigned long start = millis();
    
    myTimer.set(300, millis, [](void* obj) {
      Test29* self = (Test29*)obj;
      self->count++;
      if (self->count == 1) self->firstTime = millis();
      Serial.print("  [Test29] Callback #");
      Serial.println(self->count);
    }, true);  // isPre = true
    
    while (count < 3 && millis() - start < 1000) {
      myTimer.check();
      
    }
    
    uint32_t delta = firstTime - start;
    Serial.print("\nВремя до 1-го: ");
    Serial.print(delta);
    Serial.println("ms");
    
    (delta < 50) ? passed("Немедленный запуск!") : failed();
  }
};

// =========================================================================
// ТЕСТ 30: isPre в конструкторе
// =========================================================================
class Test30 : public TestBase {
public:
  Timer myTimer;
  unsigned long firstTime = 0;
  
  Test30() : TestBase("TEST 30: Конструктор isPre=true"),
             myTimer(250, millis, [](void* obj) {
               Test30* self = (Test30*)obj;
               self->count++;
               if (self->count == 1) self->firstTime = millis();
               Serial.print("  [Test30] Callback #");
               Serial.println(self->count);
             }, true) {  // isPre в конструкторе
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 30: Конструктор isPre=true ==========");
    
    count = 0;
    unsigned long start = millis();
    
    while (count < 3 && millis() - start < 1000) {
      myTimer.check();
      
    }
    
    uint32_t delta = firstTime - start;
    Serial.print("\nВремя до 1-го: ");
    Serial.print(delta);
    Serial.println("ms");
    
    (delta < 50) ? passed("Конструктор isPre работает!") : failed();
  }
};

// =========================================================================
// ТЕСТ 31: forCount + resetToStart внутри
// =========================================================================
class Test31 : public TestBase {
public:
  Timer myTimer;
  
  Test31() : TestBase("TEST 31: forCount + reset"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 31: forCount + resetToStart ==========");
    Serial.println("Сбрасываем счетчик на 2-й итерации\n");
    
    count = 0;
    myTimer.forCount(180, millis, [](void* obj) {
      Test31* self = (Test31*)obj;
      self->count++;
      Serial.print("  [Test31] Callback #");
      Serial.println(self->count);
      
      if (self->count == 2) {
        Serial.println("  → resetToStart (должно продолжиться)");
        self->myTimer.resetToStart();
      }
    }, 4, true);
    
    unsigned long start = millis();
    while (millis() - start < 1500) {
      myTimer.check();
      
    }
    
    Serial.print("\nВсего вызовов: ");
    Serial.println(count);
    (count >= 4) ? passed() : failed();
  }
};

// =========================================================================
// ТЕСТ 32: forTime + джиттер + переполнение
// =========================================================================
class Test32 : public TestBase {
public:
  Timer myTimer;
  
  Test32() : TestBase("TEST 32: forTime + джиттер"),
             myTimer() {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 32: forTime + джиттер ==========");
    Serial.println("forTime с задержкой 800ms\n");
    
    count = 0;
    myTimer.forTime(150, millis, [](void* obj) {
      Test32* self = (Test32*)obj;
      self->count++;
      Serial.print("  [Test32] Callback #");
      Serial.println(self->count);
    }, 1200, true);
    
    unsigned long start = millis();
    bool delayed = false;
    
    while (millis() - start < 1800) {
      myTimer.check();
      
      if (count == 2 && !delayed) {
        Serial.println("  → Задержка 800ms");
        delay(800);
        delayed = true;
      }
      
      
    }
    
    Serial.print("\nВызовов: ");
    Serial.println(count);
    (count >= 5) ? passed("Догнал!") : failed();
  }
};

// =========================================================================
// ТЕСТ 33: Комплексный - OFF + setPeriod + ON + reset
// =========================================================================
class Test33 : public TestBase {
public:
  Timer myTimer;
  
  Test33() : TestBase("TEST 33: Комплексный"),
             myTimer(300, millis, [](void* obj) {
               Test33* self = (Test33*)obj;
               self->count++;
               Serial.print("  [Test33] Callback #");
               Serial.println(self->count);
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 33: Комплексный тест ==========");
    Serial.println("OFF → setPeriod(100) → ON → resetToEnd\n");
    
    count = 0;
    myTimer.resetToStart();
    unsigned long start = millis();
    bool modified = false;
    
    while (millis() - start < 1500) {
      myTimer.check();
      
      if (count == 2 && !modified) {
        Serial.println("  → OFF + setPeriod(100) + ON + resetToEnd");
        myTimer.OFF();
        delay(10);
        myTimer.setPeriod(100);
        myTimer.ON();
        myTimer.resetToEnd();
        modified = true;
      }
      
      
    }
    
    Serial.print("\nВызовов: ");
    Serial.println(count);
    (count >= 8) ? passed("Все операции работают!") : failed();
  }
};

// =========================================================================
// ТЕСТ 34: IR-подобный тест с переключающимися irSendFuncOrder
// =========================================================================
class Test34 : public TestBase {
public:
  Timer myTimer;
  static const uint32_t BASE_PERIOD = 475;
  static const uint32_t LASER_DELAY = 175;
  static const int EXPECTED_CYCLES = 20;
  int cycleCount = 0;
  uint8_t currentFuncIndex = 0;
  uint32_t lastLaserTime = 0;
  std::function<void(void*)> irSendFuncOrder[2];
  
  Test34() : TestBase("TEST 34: IR-подобный тест (irSendFuncOrder с setPeriod в callback)"),
             myTimer(BASE_PERIOD, millis, [](void* obj) {
               Test34* self = (Test34*)obj;
               self->irSendFuncOrder[self->currentFuncIndex](obj);
             }) {
    myTimer.setObj(this);
  }
  
  void run() override {
    Serial.println("\n========== TEST 34: IR-подобный тест ==========");
    Serial.print("📋 Параметры: базовый период=");
    Serial.print(BASE_PERIOD);
    Serial.print("ms, лазерная задержка=");
    Serial.print(LASER_DELAY);
    Serial.print("ms, циклов=");
    Serial.print(EXPECTED_CYCLES);
    Serial.println("\nИмитация irSendFuncOrder с setPeriod в callback\n");
    
    cycleCount = 0;
    currentFuncIndex = 0;
    lastLaserTime = 0;
    
    // Инициализация функций
    irSendFuncOrder[0] = [](void* obj) {
      Test34* self = (Test34*)obj;
      uint32_t now = millis();
      self->cycleCount++;
      self->recordCallback(now);
      
      Serial.print("  [Test34] IR FRONT #");
      Serial.print(self->cycleCount);
      Serial.print(" at ");
      Serial.println(now);
      
      // Устанавливаем случайный период для следующего цикла
      uint16_t randomPeriod = random(300, 600);
      self->myTimer.setPeriod(randomPeriod);
      self->currentFuncIndex = 1;
    };
    
    irSendFuncOrder[1] = [](void* obj) {
      Test34* self = (Test34*)obj;
      uint32_t now = millis();
      
      if (random(100) < 70) { // 70% вероятность лазера
        // Имитация IR MEASURE
        Serial.print("  [Test34] IR MEASURE at ");
        Serial.println(now);
        
        // Установка задержки 175ms (как в машинке)
        self->myTimer.setPeriod(LASER_DELAY);
        self->lastLaserTime = now;
        
        Serial.print("  [Test34] IR LASER_HARDWARE_DELAY (175ms) at ");
        Serial.println(now);
        
      } else {
        // Имитация IR SKIP LASER
        Serial.print("  [Test34] IR SKIP LASER at ");
        Serial.println(now);
        
        self->myTimer.resetToEnd(); // Немедленное срабатывание
      }
      
      self->currentFuncIndex = 0;
    };
    
    myTimer.resetToStart();
    unsigned long start = millis();
    uint32_t timeout = BASE_PERIOD * EXPECTED_CYCLES * 3; // Запас на задержки
    
    while (cycleCount < EXPECTED_CYCLES && millis() - start < timeout) {
      myTimer.check();
    }
    
    Serial.print("\nВыполнено циклов: ");
    Serial.println(cycleCount);
    
    // Анализ соблюдения задержек
    Serial.println("\n📊 Анализ соблюдения 175ms задержки:");
    int violations = 0;
    for (int i = 1; i < callbackCount; i++) {
      if (callbackTimes[i] - callbackTimes[i-1] < LASER_DELAY - 10) {
        violations++;
        Serial.print("  ⚠️ Нарушение #");
        Serial.print(violations);
        Serial.print(": ");
        Serial.print(callbackTimes[i-1]);
        Serial.print(" → ");
        Serial.print(callbackTimes[i]);
        Serial.print(" (");
        Serial.print(callbackTimes[i] - callbackTimes[i-1]);
        Serial.println("ms)");
      }
    }
    
    if (violations == 0) {
      Serial.println("  ✓ Все задержки соблюдены!");
    } else {
      Serial.print("  ⚠️ Нарушений: ");
      Serial.println(violations);
    }
    
    // Анализ сетки
    analyzeGrid(BASE_PERIOD, false);
    
    (cycleCount >= EXPECTED_CYCLES && violations == 0) ? 
      passed("IR-подобный тест прошел!") : failed("Нарушения в задержках!");
  }
};

// =========================================================================
// MAIN
// =========================================================================
Test1 test1;
Test2 test2;
Test3 test3;
Test4 test4;
Test5 test5;
Test6 test6;
Test7 test7;
Test8 test8;
Test9 test9;
Test10 test10;
Test11 test11;
Test12 test12;
Test13 test13;
Test14 test14;
Test15 test15;
Test16 test16;
Test17 test17;
Test18 test18;
Test19 test19;
Test20 test20;
Test21 test21;
Test22 test22;
Test23 test23;
Test24 test24;
Test25 test25;
Test26 test26;
Test27 test27;
Test28 test28;
Test29 test29;
Test30 test30;
Test31 test31;
Test32 test32;
Test33 test33;
Test34 test34;

// Массив всех тестов для итоговой таблицы
TestBase* allTests[] = {
  &test1,
  &test2,
  &test3,
  &test4,
  &test5,
  &test6,
  &test7,
  &test8,
  &test9,
  &test10,
  &test11,
  &test12,
  &test13,
  &test14,
  &test15,
  &test16,
  &test17,
  &test18,
  &test19,
  &test20,
  &test21,
  &test22,
  &test23,
  &test24,
  &test25,
  &test26,
  &test27,
  &test28,
  &test29,
  &test30,
  &test31,
  &test32,
  &test33,
  &test34
};
const int totalTests = sizeof(allTests) / sizeof(allTests[0]);

// Функция вывода итоговой таблицы
void printSummaryTable() {
  int passed = 0;
  int failed = 0;
  
  Serial.println("\n\n");
  Serial.println("╔═══════════════════════════════════════════════════════════════════╗");
  Serial.println("║                    ИТОГОВАЯ ТАБЛИЦА РЕЗУЛЬТАТОВ                   ║");
  Serial.println("╠═══════╦═══════════════════════════════════════════════╦═══════════╣");
  Serial.println("║  №    ║  Название теста                               ║  Результат║");
  Serial.println("╠═══════╬═══════════════════════════════════════════════╬═══════════╣");
  
  for (int i = 0; i < totalTests; i++) {
    Serial.print("║ ");
    if (i + 1 < 10) Serial.print(" ");
    Serial.print(i + 1);
    Serial.print("    ║ ");
    
    // Название теста (47 символов)
    String testName = String(allTests[i]->name);
    Serial.print(testName);
    for (int j = testName.length(); j < 46; j++) {
      Serial.print(" ");
    }
    Serial.print("║ ");
    
    // Результат
    if (allTests[i]->wasRun) {
      if (allTests[i]->testPassed) {
        Serial.println("  ✓ PASS  ║");
        passed++;
      } else {
        Serial.println("  ✗ FAIL  ║");
        failed++;
      }
    } else {
      Serial.println("  - SKIP  ║");
    }
  }
  
  Serial.println("╠═══════╩═══════════════════════════════════════════════╩═══════════╣");
  Serial.print("║  ИТОГО: ");
  Serial.print(totalTests);
  Serial.print(" тестов | Прошло: ");
  Serial.print(passed);
  Serial.print(" | Провалено: ");
  Serial.print(failed);
  
  // Дополняем пробелами до конца строки
  int remaining = 67 - 19 - String(totalTests).length() - 11 - String(passed).length() - 14 - String(failed).length();
  for (int i = 0; i < remaining; i++) {
    Serial.print(" ");
  }
  Serial.println("║");
  
  Serial.println("╠═══════════════════════════════════════════════════════════════════╣");
  Serial.print("║  УСПЕШНОСТЬ: ");
  
  float successRate = (float)passed / totalTests * 100.0;
  Serial.print(successRate, 1);
  Serial.print("%");
  
  remaining = 67 - 15 - 5; // 5 для процентов с десятичной
  for (int i = 0; i < remaining; i++) {
    Serial.print(" ");
  }
  Serial.println("║");
  
  Serial.println("╚═══════════════════════════════════════════════════════════════════╝");
  
  if (passed == totalTests) {
    Serial.println("\n🎉 ВСЕ ТЕСТЫ ПРОШЛИ УСПЕШНО! 🎉\n");
  } else if (failed > 0) {
    Serial.println("\n⚠️  ВНИМАНИЕ: Обнаружены проблемы! ⚠️\n");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔═══════════════════════════════════════════════════════════╗");
  Serial.println("║     TimerStatic - УЛУЧШЕННЫЙ ТЕСТ с анализом сетки      ║");
  Serial.println("║   📊 Детальный анализ джиттера и смещений               ║");
  Serial.println("║   ⚠️  Предупреждения о вызовах подряд                    ║");
  Serial.println("║   📋 Полные параметры в названиях тестов                ║");
  Serial.println("╚═══════════════════════════════════════════════════════════╝");
  
  delay(500);
  
  // test1.run();
  // test2.run();
  // test3.run();
  // test4.run();
  // test5.run();
  // test6.run();
  // test7.run();
  // test8.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА A: ON/OFF КОНТРОЛЬ                      ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // test9.run();
  // test10.run();
  // test11.run();
  // test12.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА B: УПРАВЛЕНИЕ ПЕРИОДОМ                  ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // test13.run();
  // test14.run();
  // // test15.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА C: ОДНОРАЗОВЫЙ ТАЙМЕР delay()           ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // test16.run();
  // test17.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА D: ОГРАНИЧЕННЫЕ ТАЙМЕРЫ                 ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // test18.run();
  // test19.run();
  // test20.run();
  // test21.run();
  // test22.run();
  // test23.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА E: ПЕРЕКОНФИГУРАЦИЯ                     ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // test24.run();
  // test25.run();
  // test26.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА F: ИНФОРМАЦИОННЫЕ МЕТОДЫ                ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // // test27.run();
  // // test28.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА G: isPre ПАРАМЕТР                       ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // test29.run();
  // test30.run();
  
  // Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
  // Serial.println("║           ГРУППА H: СЛОЖНЫЕ СЦЕНАРИИ                     ║");
  // Serial.println("╚═══════════════════════════════════════════════════════════╝");
  // test31.run();
  // test32.run();
  // test33.run();
  test34.run();
  
  // Выводим итоговую таблицу результатов
  printSummaryTable();
  
  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════════════════════════════╗");
  Serial.println("║              ВСЕ ТЕСТЫ ЗАВЕРШЕНЫ!                         ║");
  Serial.println("╚═══════════════════════════════════════════════════════════╝");
}

void loop() {
  // Тесты выполняются один раз в setup()
}
