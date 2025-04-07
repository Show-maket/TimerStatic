#include "TimerStatic.h"
#include <vector>

class BlinkClass {
    uint8_t pin;
    uint16_t time;
    uint16_t blinkTimerDelays[2];
    bool blinkFlag = false;
public:
    Timer blink;
    BlinkClass():blinkTimerDelays({5, 995}){}
    void begin(uint8_t pin, uint16_t time = 995) {
        this->pin = pin;
        this->time = time;
        this->blinkTimerDelays[1] = time;

        pinMode(pin, OUTPUT);
        blink.setObj(this);
        blink.set(blinkTimerDelays[0], millis, [](void *obj){
            BlinkClass* self = (BlinkClass*)obj;
            digitalToggle(PC13);
            self->blink.setPeriod(self->blinkTimerDelays[self->blinkFlag = !self->blinkFlag]);
        });
    }
    // Запрет копирования
    BlinkClass(const BlinkClass&) = delete;
    BlinkClass& operator=(const BlinkClass&) = delete;
    // Разрешение перемещения
    BlinkClass(BlinkClass&& other) noexcept
        : pin(other.pin),
          time(other.time),
          blinkFlag(other.blinkFlag),
          blink(std::move(other.blink)) // перемещаем Timer
    {
        blinkTimerDelays[0] = other.blinkTimerDelays[0];
        blinkTimerDelays[1] = other.blinkTimerDelays[1];

        // Обновляем obj у таймера на this
        blink.setObj(this);
    }

    BlinkClass& operator=(BlinkClass&& other) noexcept {
        if (this != &other) {
            pin = other.pin;
            time = other.time;
            blinkFlag = other.blinkFlag;
            blink = std::move(other.blink);
            blinkTimerDelays[0] = other.blinkTimerDelays[0];
            blinkTimerDelays[1] = other.blinkTimerDelays[1];

            blink.setObj(this);
        }
        return *this;
    }
};

//todo: Протестить динамическое поведение таймера внутри контейнера
std::vector<BlinkClass> arrStatic;

void tmpFunc(){
    std::vector<BlinkClass> arr;
    arr.emplace_back();
    arr[0].begin(LED_BUILTIN, 100);
    arrStatic = std::move(arr);
}

void setup()
{
    Serial.begin(15200);
    tmpFunc();
}

void loop()
{
    Timer::tick();
}