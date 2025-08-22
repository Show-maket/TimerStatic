/*
  ButtonDebounce - Антидребезг кнопок с помощью таймеров
  
  Этот пример демонстрирует правильную реализацию антидребезга
  кнопок с помощью библиотеки TimerStatic без блокирующих конструкций.
  
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

// Переменные для антидребезга
bool lastButtonState = HIGH;
bool buttonPressed = false;
bool isButtonProcessed = false;
const unsigned long DEBOUNCE_DELAY = 50; // 50мс для антидребезга

// Таймер для мигания светодиодом каждые 300мс
Timer blinkTimer(300, millis, []() {
  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState);
});

// Таймер для антидребезга кнопки
Timer debounceTimer;

// Таймер для демонстрации задержки
Timer demoTimer;

// Счетчик нажатий
int pressCount = 0;

void setup() {
  // Инициализируем пины
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Инициализируем Serial для отладки
  Serial.begin(9600);
  Serial.println("TimerStatic Пример Антидребезга Кнопок");
  Serial.println("Нажмите кнопку для демонстрации");
  
  // Запускаем мигание светодиодом
  blinkTimer.ON();
}

void loop() {
  // Проверяем таймеры
  debounceTimer.check();
  demoTimer.check();
  blinkTimer.check();
  
  // Читаем состояние кнопки
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Обрабатываем нажатие кнопки (только при изменении состояния)
  if (currentButtonState != lastButtonState) {
    // Сбрасываем флаг обработки при изменении состояния
    isButtonProcessed = false;
    lastButtonState = currentButtonState;
  }
  
  // Если кнопка нажата и еще не обработана
  if (currentButtonState == LOW && !isButtonProcessed) {
    // Запускаем таймер антидребезга
    debounceTimer.delay(DEBOUNCE_DELAY, millis, []() {
      // После задержки проверяем, что кнопка все еще нажата
      if (digitalRead(BUTTON_PIN) == LOW) {
        // Кнопка действительно нажата - обрабатываем
        buttonPressed = true;
        isButtonProcessed = true;
        pressCount++;
        
        Serial.print("Кнопка нажата! Счетчик: ");
        Serial.println(pressCount);
        
        // Демонстрируем задержку с помощью таймера
        Serial.println("Запускаем 2-секундную задержку...");
        demoTimer.delay(2000, millis, []() {
          Serial.println("2 секунды прошли! Во время ожидания светодиод мигал!");
        });
      }
    });
  }
  
  // Если кнопка отпущена, сбрасываем флаги
  if (currentButtonState == HIGH) {
    buttonPressed = false;
    isButtonProcessed = false;
  }
  
  // Здесь может выполняться другой код
  // Например, чтение датчиков, обработка данных и т.д.
  // Код выполняется непрерывно, независимо от состояния кнопки
}
