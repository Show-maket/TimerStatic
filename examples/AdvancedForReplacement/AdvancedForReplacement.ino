/*
  AdvancedForReplacement - Продвинутая замена циклов for с помощью таймеров
  
  Этот пример демонстрирует более сложные сценарии замены циклов for,
  включая вложенные циклы и циклы с разными интервалами.
  
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

// Таймер для мигания светодиодом каждые 100мс
Timer blinkTimer(100, millis, []() {
  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState);
});

// Таймеры для замены циклов
Timer outerLoopTimer;
Timer innerLoopTimer;

// Счетчики для демонстрации
int outerCounter = 0;
int innerCounter = 0;
bool isOuterLoopRunning = false;

void setup() {
  // Инициализируем пины
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Инициализируем Serial для отладки
  Serial.begin(9600);
  Serial.println("TimerStatic Продвинутый Пример Замены Циклов For");
  Serial.println("Нажмите кнопку для демонстрации вложенных циклов");
  
  // Запускаем мигание светодиодом
  blinkTimer.ON();
}

void loop() {
  // Проверяем таймеры
  outerLoopTimer.check();
  innerLoopTimer.check();
  blinkTimer.check();
  
  // Проверяем нажатие кнопки
  if (digitalRead(BUTTON_PIN) == LOW && !isOuterLoopRunning) {
    // Демонстрируем замену вложенных циклов for
    Serial.println("Кнопка нажата! Запускаем вложенные циклы...");
    
    // Сбрасываем счетчики
    outerCounter = 0;
    innerCounter = 0;
    isOuterLoopRunning = true;
    
    // Внешний цикл: 3 итерации с интервалом 2 секунды
    outerLoopTimer.forCount(2000, millis, []() {
      outerCounter++;
      Serial.print("Внешний цикл: итерация ");
      Serial.print(outerCounter);
      Serial.println("/3");
      
      // Внутренний цикл: 4 итерации с интервалом 500мс
      innerCounter = 0;
      innerLoopTimer.forCount(500, millis, []() {
        innerCounter++;
        Serial.print("  Внутренний цикл: итерация ");
        Serial.print(innerCounter);
        Serial.println("/4");
        
        if (innerCounter >= 4) {
          Serial.println("  Внутренний цикл завершен");
        }
      }, 4);
      
      if (outerCounter >= 3) {
        Serial.println("Внешний цикл завершен!");
        Serial.println("Все циклы завершены!");
        isOuterLoopRunning = false;
      }
    }, 3);
    
    // Запускаем таймер для антидребезга кнопки
    outerLoopTimer.delay(100, millis, []() {
      // После короткой задержки проверяем состояние кнопки
      if (digitalRead(BUTTON_PIN) == HIGH) {
        Serial.println("Кнопка отпущена, готов к следующему нажатию");
      }
    });
  }
  
  // Здесь может выполняться другой код во время выполнения циклов
  // Например, чтение датчиков, обработка данных и т.д.
}
