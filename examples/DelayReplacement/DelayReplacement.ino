/*
  DelayReplacement - Замена функции delay с помощью таймера
  
  Этот пример демонстрирует, как заменить блокирующую функцию delay()
  на неблокирующий таймер, что позволяет выполнять другие задачи
  во время ожидания.
  
  Схема подключения:
  - Светодиод подключен к пину 13 (или LED_BUILTIN)
  - Кнопка подключена к пину 2 с подтягивающим резистором
  
  Создано DashyFox
  Этот пример находится в общественном достоянии.
*/

#include "TimerStatic.h"

// Пин кнопки
const int BUTTON_PIN = 2;

// Состояние светодиода
bool ledState = false;

// Таймер для замены delay - срабатывает через 2 секунды
Timer delayTimer;

// Таймер для мигания светодиодом каждые 500мс
Timer blinkTimer(500, millis, []() {
  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState);
});

void setup() {
  // Инициализируем пины
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Инициализируем Serial для отладки
  Serial.begin(9600);
  Serial.println("TimerStatic Пример Замены Delay");
  Serial.println("Нажмите кнопку для демонстрации delay");
  
  // Запускаем мигание светодиодом
  blinkTimer.ON();
}

void loop() {
  // Проверяем таймеры
  delayTimer.check();
  blinkTimer.check();
  
  // Проверяем нажатие кнопки
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Демонстрируем замену delay
    Serial.println("Кнопка нажата! Запускаем 2-секундную задержку...");
    
    // Вместо delay(2000) используем таймер
    delayTimer.delay(2000, millis, []() {
      Serial.println("2 секунды прошли! Задержка завершена.");
      Serial.println("Во время ожидания светодиод продолжал мигать!");
    });
    
    // Запускаем таймер для антидребезга кнопки
    delayTimer.delay(100, millis, []() {
      // После короткой задержки проверяем состояние кнопки
      if (digitalRead(BUTTON_PIN) == HIGH) {
        Serial.println("Кнопка отпущена, готов к следующему нажатию");
      }
    });
  }
  
  // Здесь может выполняться другой код во время ожидания
  // Например, чтение датчиков, обработка данных и т.д.
}
