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

class Interrupt9;

class Timers
{
    private:
        Interrupt9* int9;
        Timer9 arm9_timers[4];

        uint16_t get_control(int index);
        void set_counter(int index, uint16_t value);
        void set_control(int index, uint16_t value);

        void handle_overflow(int index);
    public:
        Timers(Interrupt9* int9);

        void reset();
        void run();

        uint16_t arm9_read16(uint32_t addr);
        void arm9_write16(uint32_t addr, uint16_t value);
};

#endif // TIMERS_HPP
