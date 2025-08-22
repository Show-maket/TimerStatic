/*
  ForLoopReplacement - Замена циклов for с помощью таймера
  
  Этот пример демонстрирует, как заменить блокирующие циклы for
  на неблокирующие таймеры, что позволяет выполнять другие задачи
  во время выполнения цикла.
  
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

// Таймер для мигания светодиодом каждые 200мс
Timer blinkTimer(200, millis, []() {
  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState);
});

// Таймер для замены цикла for
Timer forTimer;

// Счетчик для демонстрации
int counter = 0;

void setup() {
  // Инициализируем пины
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Инициализируем Serial для отладки
  Serial.begin(9600);
  Serial.println("TimerStatic Пример Замены Циклов For");
  Serial.println("Нажмите кнопку для демонстрации for");
  
  // Запускаем мигание светодиодом
  blinkTimer.ON();
}

void loop() {
  // Проверяем таймеры
  forTimer.check();
  blinkTimer.check();
  
  // Проверяем нажатие кнопки
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Демонстрируем замену цикла for
    Serial.println("Кнопка нажата! Запускаем цикл for...");
    
    // Сбрасываем счетчик
    counter = 0;
    
    // Вместо for(int i=0; i<5; i++) используем таймер
    // Выполняем 5 итераций с интервалом 1 секунда
    forTimer.forCount(1000, millis, []() {
      counter++;
      Serial.print("Итерация ");
      Serial.print(counter);
      Serial.println("/5");
      
      if (counter >= 5) {
        Serial.println("Цикл завершен!");
        Serial.println("Во время выполнения светодиод продолжал мигать!");
      }
    }, 5); // 5 итераций
    
    // Запускаем таймер для антидребезга кнопки
    forTimer.delay(100, millis, []() {
      // После короткой задержки проверяем состояние кнопки
      if (digitalRead(BUTTON_PIN) == HIGH) {
        Serial.println("Кнопка отпущена, готов к следующему нажатию");
      }
    });
  }
  
  // Здесь может выполняться другой код во время выполнения цикла
  // Например, чтение датчиков, обработка данных и т.д.
}
