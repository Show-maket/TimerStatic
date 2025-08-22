/*
  AVRCompatibility - Пример совместимости с AVR
  
  Этот пример демонстрирует, как использовать библиотеку TimerStatic
  на платах AVR, которые не поддерживают std::function и лямбда-выражения.
  
  Схема подключения:
  - Светодиод подключен к пину 13 (или LED_BUILTIN)
  
  Создано DashyFox
  Этот пример находится в общественном достоянии.
*/

#include "TimerStatic.h"

// Глобальные переменные для совместимости с AVR
int ledState = LOW;
unsigned long lastBlinkTime = 0;

// Функция обратного вызова для плат AVR
void blinkCallback(void* obj) {
  // Приводим указатель объекта обратно к исходному типу
  int* ledPin = (int*)obj;
  
  // Переключаем состояние светодиода
  ledState = !ledState;
  digitalWrite(*ledPin, ledState);
  
  // Опционально: выводим в Serial, если доступно
  #ifdef __AVR__
  // Здесь может быть код, специфичный для AVR
  #endif
}

// Создаём объект таймера
Timer blinkTimer;

void setup() {
  // Инициализируем пин светодиода
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Инициализируем Serial
  Serial.begin(9600);
  Serial.println("TimerStatic Пример Совместимости с AVR");
  
  // Настраиваем таймер с функцией обратного вызова и пином светодиода как контекстом
  blinkTimer.setObj(&LED_BUILTIN);
  blinkTimer.set(1000, millis, blinkCallback);
}

void loop() {
  // Проверяем, должен ли сработать таймер
  blinkTimer.check();
  
  // Альтернатива: используйте глобальный tick для обновления всех таймеров
  // Timer::tick();
  
  // Здесь может быть ваш другой код
  // Таймер будет продолжать работать без блокировки
}
