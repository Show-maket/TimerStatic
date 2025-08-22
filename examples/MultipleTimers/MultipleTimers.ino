/*
  MultipleTimers - Пример с несколькими таймерами
  
  Этот пример демонстрирует, как использовать несколько таймеров
  с разными периодами и типами обратных вызовов.
  
  Схема подключения:
  - Светодиод подключен к пину 13 (или LED_BUILTIN)
  - Светодиод подключен к пину 12
  - Светодиод подключен к пину 11
  
  Создано DashyFox
  Этот пример находится в общественном достоянии.
*/

#include "TimerStatic.h"

// Быстро мигающий светодиод (каждые 200мс)
Timer fastTimer(200, millis, []() {
  digitalWrite(13, !digitalRead(13));
});

// Средне мигающий светодиод (каждые 500мс)
Timer mediumTimer(500, millis, []() {
  digitalWrite(12, !digitalRead(12));
});

// Медленно мигающий светодиод (каждые 1000мс)
Timer slowTimer(1000, millis, []() {
  digitalWrite(11, !digitalRead(11));
});

// Таймер однократной задержки (выводит сообщение через 3 секунды)
Timer delayTimer;
bool delayMessagePrinted = false;

void setup() {
  // Инициализируем пины светодиодов
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(11, OUTPUT);
  
  // Инициализируем Serial
  Serial.begin(9600);
  Serial.println("TimerStatic Пример Нескольких Таймеров");
  
  // Настраиваем таймер задержки для вывода сообщения через 3 секунды
  delayTimer.delay(3000, millis, []() {
    Serial.println("Прошло 3 секунды!");
    delayMessagePrinted = true;
  });
}

void loop() {
  //* Используйте глобальный tick для обновления всех таймеров сразу
  Timer::tick();

  //* Альтернатива: Проверяем все таймеры индивидуально
  // fastTimer.check();
  // mediumTimer.check();
  // slowTimer.check();
  // delayTimer.check();
  
  
  //* Здесь может быть ваш другой код
  //* Таймеры будут продолжать работать без блокировки
}
