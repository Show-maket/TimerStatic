# TimerStatic

**TimerStatic** — лёгкая Arduino-библиотека для управления множеством таймеров с использованием `millis()` или другой пользовательской функции времени.

Поддерживает: `delay`, `set`, `forCount`, `forTime`, `setCallback`, передачу параметров, глобальное обновление всех таймеров через `Timer::tick()`.

---

## Подключение
```cpp
#include <TimerStatic.h>
```

В `loop()` обязательно:
```cpp
void loop() {
  Timer::tick();
}
```

---

## Примеры для AVR (только указатели на функции C++)

### Простой повторяющийся таймер
```cpp
void toggle(void *) {
  digitalWrite(13, !digitalRead(13));
}

Timer t;

void setup() {
  pinMode(13, OUTPUT);
  t.set(500, millis, toggle);
}
```

### Таймер внутри класса (AVR, C++11)
```cpp
class LED {
  Timer t;
  uint8_t pin;
  bool state = false;

  static void toggle(void *ctx) {
    LED *self = static_cast<LED*>(ctx);
    self->state = !self->state;
    digitalWrite(self->pin, self->state);
  }

public:
  LED(uint8_t p, uint32_t ms) : pin(p) {
    pinMode(pin, OUTPUT);
    t.setObj(this);
    t.set(ms, millis, toggle);
  }
};

LED led(13, 500);
```

---

## Примеры для ARM/ESP/STM32 (с лямбдами, std::function)

### Прямо в конструкторе (бесконечный таймер)
```cpp
Timer t(1000, millis, [] (void *) {
  Serial.println("Tick");
});
```

### Отложенное действие (delay)
```cpp
Timer t;

void setup() {
  t.delay_std(3000, millis, []() {
    Serial.println("3 секунды прошло");
  });
}
```

### Несколько повторов
```cpp
int counter = 0;
Timer t;

void setup() {
  t.forCount_std(1000, millis, []() {
    Serial.println("Пинг");
  }, 5);
}
```

---

## Примеры с передачей параметров

### Передача `this` в нестатический метод (через лямбду)
```cpp
class Device {
  Timer t;
  int pin;

public:
  Device(int p): pin(p) {
    pinMode(pin, OUTPUT);
    t.set_std(1000, millis, [this]() {
      digitalWrite(pin, !digitalRead(pin));
    });
  }
};

Device d(13);
```

### Передача пользовательской структуры
```cpp
struct Data {
  int pin;
};

Timer t;
Data d = {13};

void blink(void *ctx) {
  Data *data = (Data*)ctx;
  digitalWrite(data->pin, !digitalRead(data->pin));
}

void setup() {
  pinMode(d.pin, OUTPUT);
  t.setObj(&d);
  t.set(500, millis, blink);
}
```

---

## Альтернативные варианты записи

### Без колбэка — только таймер и check в loop (ручной вызов)
```cpp
Timer t;
void setup() {
  t.set(1000, millis, callback);
}
void loop() {
  t.check(); // вместо Timer::tick()
}
```

### Таймер с немедленным первым вызовом
```cpp
Timer t;
t.set(1000, millis, cb, true); // вызов cb сразу, потом каждые 1000 мс
```

---

## Специфические возможности

### Программная остановка
```cpp
t.OFF();  // поставить на паузу
t.ON();   // продолжить
```

### Один раз, потом стоп
```cpp
t.delay(5000, millis, cb);  // только один вызов через 5 секунд
```

---

## Советы
- Для AVR используйте указатели на функции и `setObj()` для передачи контекста.
- Для ESP32/STM32/ARM используйте `set_std()` и лямбды с захватом.
- Один `Timer::tick()` обновляет **все** таймеры.
- Не используйте блокирующие `delay()` — это библиотека для **неблокирующего** программирования.

---

## Совместимость

| Платформа | std::function / лямбды | set / delay / forCount | Контекст `this` |
|----------|--------------------------|-------------------------|------------------|
| AVR      | ❌ только `void (*)(void*)` | ✅                      | через setObj     |
| STM32    | ✅                        | ✅                      | через лямбду     |
| ESP32    | ✅                        | ✅                      | через лямбду     |
| RP2040   | ✅                        | ✅                      | через лямбду     |

---

## Поддерживаемые методы

- `set`, `set_std`
- `delay`, `delay_std`
- `forCount`, `forCount_std`
- `forTime`, `forTime_std`
- `setCallback`, `setObj`, `setTimeFunc`
- `ON()`, `OFF()`, `isRun()`, `isForLast()`
- `setPeriod()`, `getPeriod()`
- `Timer::tick()` — обновление всех таймеров

---

Лёгкая, универсальная библиотека для управления многозадачностью без прерываний и блокировок.