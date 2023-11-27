class Timer {
private:
    static inline Timer* head = nullptr;
    static inline Timer* last = nullptr;
    static inline uint32_t lifeShortenerValStandart = 1;

public:
    static void tick() {
        Timer* current = Timer::head;
        while (current != nullptr) {
            current->check();
            current = current->next;
        }
    }

private:
    Timer* next;
    unsigned long nextTimeTrigger, period;
    unsigned long(*t_func)();
    void(*callback)();
    bool isRun = true;
    bool isInf;
    uint32_t life = 0;
    // void(*lifeShortener)(Timer* t) = Timer::lifeShortenerCount;
    uint32_t* lifeShortenerVal = &lifeShortenerValStandart;

    // static void lifeShortenerCount(Timer* timer) {
    //     timer->life--;
    //     if (!timer->isInf && timer->life < timer->life - 1) {
    //         timer->isRun = false;
    //     }
    // }
    // static void lifeShortenerTime(Timer* timer) {
    //     timer->life -= timer->period;
    //     if (!timer->isInf && timer->life < timer->life - timer->period) {
    //         timer->isRun = false;
    //     }
    // }

public:

    Timer(unsigned long time = 0, unsigned long(*t_func)() = nullptr, void(*callback)() = nullptr, bool isPre = false) {
        if (Timer::head == nullptr) {
            Timer::head = this;
        }
        if (last != nullptr) {
            last->next = this;
        }
        last = this;

        this->t_func = t_func;
        this->isInf = true;
        if (!callback || !t_func) {
            isRun = false;
        } else {
            this->nextTimeTrigger = isPre ? t_func() - period : t_func();
        }
        this->period = time;
        this->callback = callback;
    }

    void check() {
        if (callback && t_func) {
            if (t_func() - nextTimeTrigger >= period && isRun) {
                do {
                    nextTimeTrigger += period;
                    if (nextTimeTrigger < period) break;          // переполнение uint32_t
                } while (nextTimeTrigger < t_func() - period);   // защита от пропуска шага
                callback();
                // Serial.println((String)life + "   " + (String)period);

                if (!this->isInf && this->life < this->life - *lifeShortenerVal) {
                    this->isRun = false;
                }
                this->life -= *lifeShortenerVal;
            }
        }
    }

    void resetToStart() {
        nextTimeTrigger = t_func();
    }
    void resetToEnd() {
        nextTimeTrigger = t_func() - period;
    }

    void ON() {
        this->isRun = true;
    }
    void OFF() {
        this->isRun = false;
    }

    void delay(uint32_t time = 0, unsigned long(*t_func)() = nullptr, void(*callback)() = nullptr) {
        this->lifeShortenerVal = &lifeShortenerValStandart;
        this->period = time;
        this->t_func = t_func;
        this->callback = callback;
        this->life = 0;
        this->isRun = true;
        this->isInf = false;
        this->nextTimeTrigger = t_func();
    }
    void forCount(uint32_t time, unsigned long(*t_func)(), void(*callback)(), uint16_t lifeCount, bool isPre = true) {

        this->lifeShortenerVal = &lifeShortenerValStandart;
        this->period = time;
        this->t_func = t_func;
        this->callback = callback;
        this->life = lifeCount - 1;
        this->isInf = false;
        this->isRun = lifeCount != 0;
        this->nextTimeTrigger = isPre ? t_func() - period : t_func();
    }

    void forTime(uint32_t time, unsigned long(*t_func)(), void(*callback)(), uint32_t lifeTime, bool isPre = true) {
        this->lifeShortenerVal = (uint32_t*)&this->period;
        this->period = time;
        this->t_func = t_func;
        this->callback = callback;
        this->life = lifeTime;
        this->isInf = false;
        this->isRun = lifeTime != 0;
        this->nextTimeTrigger = isPre ? t_func() - period : t_func();
    }

    bool isForLast() {
        return this->life <= period;
    }

    void set(unsigned long time, unsigned long(*t_func)(), void(*callback)(), bool isPre = false) {
        this->period = time;
        this->t_func = t_func;
        this->callback = callback;
        this->life = 0;
        this->isInf = true;
        this->isRun = true;
        this->nextTimeTrigger = isPre ? t_func() - period : t_func();
    }

    void setPeriod(uint32_t val) {
        this->period = val;
    }

    void setCallback(void(*callback)()) {
        this->callback = callback;
    }

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
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀v1.3⠀⠀⠀ ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//**************************************************
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//

///////////////////////////////////////////////////////////////////////////////////////////////////////*/