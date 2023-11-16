class Timer {
public:
    static inline Timer* head = nullptr;
    static inline Timer* last = nullptr;

    static void tick() {
        Timer* current = Timer::head;
        while (current != nullptr) {
            current->check();
            current = current->next;
        }
    }

    unsigned long nextTimeTrigger, period;
    unsigned long(*t_func)();
    void(*callback)();
    bool isRun = true;
    uint16_t count = 5;
    bool isInf;


    Timer* next;

    Timer(unsigned long time = 0, unsigned long(*t_func)() = nullptr, void(*callback)() = nullptr, bool isPre = false, bool isInf = true) {
        if (Timer::head == nullptr) {
            Timer::head = this;
        }
        if (last != nullptr) {
            last->next = this;
        }
        last = this;

        if (!callback && !t_func) { isRun = false; }
        this->period = time;
        this->t_func = t_func;
        this->callback = callback;
        this->isInf = true;
        this->nextTimeTrigger = isPre ? t_func()-period : t_func();
    }

    void check() {
        if (callback && t_func) {
            if (t_func() - nextTimeTrigger >= period && isRun) {
                callback();
                count--;
                if (count < count - 1 && !isInf) {
                    isRun = false;
                }
                do {
                    nextTimeTrigger += period;
                    if (nextTimeTrigger < period) break;          // переполнение uint32_t
                } while (nextTimeTrigger < t_func() - period);   // защита от пропуска шага
            }
        }
    }

    void resetToStart() {
        nextTimeTrigger = t_func();
    }
    void resetToEnd() {
        nextTimeTrigger = t_func() - period;
    }

    void delay(uint32_t time = 0, unsigned long(*t_func)() = nullptr, void(*callback)() = nullptr) {
        this->period = time;
        this->t_func = t_func;
        this->callback = callback;
        this->count = 1;
        this->isRun = true;
        this->isInf = false;

        this->nextTimeTrigger = t_func();
    }
    void forCount(uint32_t time, unsigned long(*t_func)(), void(*callback)(), uint16_t count, bool isPre = true) {
        this->period = time;
        this->t_func = t_func;
        this->callback = callback;
        this->count = count;
        this->isInf = false;
        this->isRun = true;

        this->nextTimeTrigger = isPre ? t_func()-period : t_func();
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
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀v1.1⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//**************************************************
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀//

///////////////////////////////////////////////////////////////////////////////////////////////////////*/