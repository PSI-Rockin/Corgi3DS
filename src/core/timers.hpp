#ifndef TIMERS_HPP
#define TIMERS_HPP
#include <cstdint>

struct Timer9
{
    uint32_t counter;
    uint32_t clocks;
    uint32_t prescalar;
    bool countup;
    bool overflow_irq;
    bool enabled;
};

struct Timer11
{
    uint32_t load;
    uint32_t clocks;
    uint32_t counter;
    uint32_t prescalar;
    bool int_enabled;
    bool watchdog_mode;
    bool auto_reload;
    bool enabled;
    bool int_flag;
};

class Interrupt9;
class MPCore_PMR;
class Emulator;

class Timers
{
    private:
        Interrupt9* int9;
        MPCore_PMR* pmr;
        Emulator* e;
        Timer9 arm9_timers[4];
        Timer11 arm11_timers[8];

        uint16_t get_control(int index);
        void set_counter(int index, uint16_t value);
        void set_control(int index, uint16_t value);

        void overflow_check11(int index);
        void handle_overflow(int index);
    public:
        Timers(Interrupt9* int9, MPCore_PMR* pmr, Emulator* e);

        void reset();
        void run(int cycles11, int cycles9);

        uint16_t arm9_read16(uint32_t addr);
        void arm9_write16(uint32_t addr, uint16_t value);

        uint32_t arm11_get_load(int id);
        uint32_t arm11_get_counter(int id, int delta);
        uint32_t arm11_get_control(int id);
        uint32_t arm11_get_int_status(int id);

        void arm11_set_load(int id, uint32_t value);
        void arm11_set_counter(int id, uint32_t value);
        void arm11_set_control(int id, uint32_t value);
        void arm11_set_int_status(int id, uint32_t value);
};

#endif // TIMERS_HPP
