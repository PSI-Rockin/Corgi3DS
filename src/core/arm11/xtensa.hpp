#ifndef XTENSA_HPP
#define XTENSA_HPP
#include <cstdint>
#include <cstdio>

class WiFi;

struct XtensaProgramState
{
    uint8_t int_level;
    bool exception;
    bool user_vector_mode;
    uint8_t ring;
    uint8_t old_window_base;
    uint8_t call_inc;
    bool window_overflow_detection;
};

class Xtensa
{
    private:
        WiFi* wifi;
        uint32_t pc;

        //The number of "address registers" (GPRs) is normally 16.
        //However, the AR6014 supports a window extension, which extends the number of total registers
        //while keeping 16 visible at a time. This saves time when entering and exiting subroutines,
        //as the window "rotates", hiding parent registers.
        //TODO: The window extension provides either 32 or 64 GPRs depending on implementation.
        //How many are actually on the AR6014?
        //uint32_t gpr[64];
        uint32_t gpr[1024];

        uint32_t lbeg, lend, lcount;
        uint32_t sar;

        uint32_t litbase;

        XtensaProgramState ps;
        XtensaProgramState eps[7];

        uint8_t window_base;
        uint32_t window_start;

        uint32_t epc[7];
        uint32_t excsave[7];
        uint32_t intenable;
        uint32_t interrupt;

        bool halted;
    public:
        Xtensa(WiFi* wifi);

        uint8_t fetch_byte();
        uint16_t fetch_word();

        uint8_t read8(uint32_t addr);
        uint16_t read16(uint32_t addr);
        uint32_t read32(uint32_t addr);
        void write8(uint32_t addr, uint8_t value);
        void write16(uint32_t addr, uint16_t value);
        void write32(uint32_t addr, uint32_t value);

        void reset();
        void run(int cycles);
        void print_state();

        void halt();
        void unhalt();

        void send_irq(int id);
        void clear_irq(int id);

        void jp(uint32_t addr);
        void branch(int offset);

        uint32_t get_pc();
        uint32_t get_gpr(int index);
        void set_gpr(int index, uint32_t value);

        uint32_t get_xsr(int index);
        void set_xsr(int index, uint32_t value);

        bool extended_l32r();
        uint32_t get_litbase();

        uint32_t get_sar();
        void set_sar(uint32_t value);

        uint32_t get_ps();
        void set_ps(uint32_t value);

        void setup_loop(int count, int offset, bool cond);
        void windowed_call(uint32_t addr, int inc);
        void entry(int sp, int frame);
        void windowed_ret();
        void rfi(int level);
};

inline void Xtensa::halt()
{
    halted = true;
}

inline void Xtensa::unhalt()
{
    halted = false;
}

inline uint32_t Xtensa::get_pc()
{
    return pc;
}

inline uint32_t Xtensa::get_gpr(int index)
{
    return gpr[index + (window_base << 2)];
}

inline void Xtensa::set_gpr(int index, uint32_t value)
{
    //printf("[Xtensa] Set a%d: $%08X\n", index, value);
    gpr[index + (window_base << 2)] = value;
}

inline bool Xtensa::extended_l32r()
{
    return litbase & 0x1;
}

inline uint32_t Xtensa::get_litbase()
{
    return litbase & ~0xFFF;
}

inline uint32_t Xtensa::get_sar()
{
    return sar;
}

inline void Xtensa::set_sar(uint32_t value)
{
    sar = value;
}

#endif // XTENSA_HPP
