#include "../common/common.hpp"
#include "wifi.hpp"
#include "xtensa.hpp"
#include "xtensa_interpreter.hpp"

Xtensa::Xtensa(WiFi* wifi) : wifi(wifi)
{

}

void Xtensa::reset()
{
    //Entry point in the ROM
    pc = 0x8E0000;
    window_base = 0;
    window_start = 0;
    halted = false;
    ps.exception = false;
    intenable = 0;
    interrupt = 0;
    litbase = 0;
}

void Xtensa::run(int cycles)
{
    while (!halted && cycles > 0)
    {
        cycles--;
        print_state();
        uint16_t instr = fetch_word();
        Xtensa_Interpreter::interpret(*this, instr);

        if (lcount > 0 && pc == lend)
        {
            //Looping is disabled during an exception
            if (!ps.exception)
            {
                pc = lbeg;
                lcount--;
            }
        }
    }
}

uint8_t Xtensa::fetch_byte()
{
    uint8_t value = wifi->read8_xtensa(pc);
    pc++;
    return value;
}

uint16_t Xtensa::fetch_word()
{
    uint16_t value = wifi->read16_xtensa(pc);
    pc += 2;
    return value;
}

uint8_t Xtensa::read8(uint32_t addr)
{
    printf("[Xtensa] Read8 $%08X\n", addr);
    return wifi->read8_xtensa(addr);
}

uint16_t Xtensa::read16(uint32_t addr)
{
    if (addr & 0x1)
        EmuException::die("[Xtensa] Invalid read16 $%08X", addr);
    printf("[Xtensa] Read16 $%08X\n", addr);
    return wifi->read16_xtensa(addr);
}

uint32_t Xtensa::read32(uint32_t addr)
{
    if (addr & 0x3)
        EmuException::die("[Xtensa] Invalid read32 $%08X", addr);
    printf("[Xtensa] Read32 $%08X\n", addr);
    return wifi->read32_xtensa(addr);
}

void Xtensa::write8(uint32_t addr, uint8_t value)
{
    printf("[Xtensa] Write8 $%08X: $%02X\n", addr, value);
    wifi->write8_xtensa(addr, value);
}

void Xtensa::write16(uint32_t addr, uint16_t value)
{
    if (addr & 0x1)
        EmuException::die("[Xtensa] Invalid write16 $%08X: $%04X", addr, value);
    printf("[Xtensa] Write16 $%08X: $%04X\n", addr, value);
    wifi->write16_xtensa(addr, value);
}

void Xtensa::write32(uint32_t addr, uint32_t value)
{
    if (addr & 0x3)
        EmuException::die("[Xtensa] Invalid write32 $%08X: $%08X", addr, value);
    if (addr >= 0x520000)
        printf("[Xtensa] Write32 $%08X: $%08X\n", addr, value);
    wifi->write32_xtensa(addr, value);
}

void Xtensa::send_irq(int id)
{
    uint32_t vector;
    int level;
    if (id == 0)
    {
        level = 1;
        vector = 0x8E0720;
    }
    else if (id < 15)
    {
        level = 2;
        vector = 0x8E0920;
    }
    else
    {
        level = 3;
        vector = 0x8E0A20;
    }
    interrupt |= 1 << id;
    if (intenable & (1 << id))
    {
        if (ps.int_level < level)
        {
            epc[level - 1] = pc;
            eps[level - 1] = ps;
            ps.int_level = level;
            ps.exception = true;
            pc = vector;
            unhalt();
        }
    }
}

void Xtensa::clear_irq(int id)
{
    interrupt &= ~(1 << id);
}

void Xtensa::jp(uint32_t addr)
{
    pc = addr;
}

void Xtensa::branch(int offset)
{
    pc += offset;
}

void Xtensa::print_state()
{
    printf("pc:$%08X\n", pc);
    /*for (int i = 0; i < 16; i++)
    {
        printf("a%d:%08X ", i, gpr[i + (window_base << 2)]);
        if (i % 4 == 3)
            printf("\n");
        else
            printf("\t");
    }*/
    //printf("Window base: $%02X start: $%08X\n", window_base, window_start);
}

uint32_t Xtensa::get_xsr(int index)
{
    uint32_t reg = 0;
    switch (index)
    {
        case 0:
            return lbeg;
        case 1:
            return lend;
        case 2:
            return lcount;
        case 3:
            return sar;
        case 178:
            return epc[1];
        case 209:
        case 210:
        case 211:
        case 212:
        case 213:
        case 214:
        case 215:
            reg = excsave[index - 209];
            break;
        case 226:
            printf("Interrupt: $%08X\n", interrupt);
            return interrupt;
        case 228:
            return intenable;
        case 230:
            return get_ps();
        default:
            printf("[Xtensa] Unrecognized XSR %d in get_xsr\n", index);
    }
    return reg;
}

void Xtensa::set_xsr(int index, uint32_t value)
{
    switch (index)
    {
        case 0:
            lbeg = value;
            break;
        case 1:
            lend = value;
            break;
        case 2:
            lcount = value;
            break;
        case 3:
            sar = value & 0x1F;
            break;
        case 5:
            litbase = value;
            break;
        case 72:
            window_base = value;
            break;
        case 73:
            window_start = value;
            break;
        case 209:
        case 210:
        case 211:
        case 212:
        case 213:
        case 214:
        case 215:
            //NOTE: ROM seems to dump exception vectors in EXCSAVE[2] and EXCSAVE[3].
            excsave[index - 209] = value;
            break;
        case 227:
            printf("[Xtensa] Write interrupt $%08X\n", value);
            interrupt &= ~value;
            break;
        case 228:
            printf("[Xtensa] Int enable: $%08X\n", value);
            intenable = value;
            break;
        case 230:
            set_ps(value);
            break;
        default:
            printf("[Xtensa] Unrecognized XSR %d in set_xsr ($%08X)\n", index, value);
    }
}

uint32_t Xtensa::get_ps()
{
    uint32_t reg = ps.int_level;
    reg |= ps.exception << 4;
    reg |= ps.user_vector_mode << 5;
    reg |= ps.ring << 6;
    reg |= ps.old_window_base << 8;
    reg |= ps.call_inc << 16;
    reg |= ps.window_overflow_detection << 18;
    return reg;
}

void Xtensa::set_ps(uint32_t value)
{
    ps.int_level = value & 0xF;
    ps.exception = (value >> 4) & 0x1;
    ps.user_vector_mode = (value >> 5) & 0x1;
    ps.ring = (value >> 6) & 0x3;
    ps.old_window_base = (value >> 8) & 0xF;
    ps.call_inc = (value >> 16) & 0x3;
    ps.window_overflow_detection = (value >> 18) & 0x1;
}

void Xtensa::setup_loop(int count, int offset, bool cond)
{
    if (count > 0)
        lcount = count - 1;
    else
        lcount = 0;
    lbeg = pc;
    lend = pc + offset + 1;

    if (!cond)
        pc = lend;
}

void Xtensa::windowed_call(uint32_t addr, int inc)
{
    set_gpr(inc << 2, (pc & 0x3FFFFFFF) | (inc << 30));
    ps.call_inc = inc;
    pc = addr;
}

void Xtensa::entry(int sp, int frame)
{
    //if (window_base + ps.call_inc >= 16)
    if (window_base + ps.call_inc >= 256)
    {
        //TODO: Overflow exception
        EmuException::die("[Xtensa] Overflow exception");
    }
    uint32_t old_sp = get_gpr(sp);
    set_gpr(sp | (ps.call_inc << 2), old_sp - frame);
    window_base += ps.call_inc;
    window_start |= 1 << window_base;
}

void Xtensa::windowed_ret()
{
    int n = get_gpr(0) >> 30;
    uint32_t new_pc = get_gpr(0) & 0x3FFFFFFF;
    new_pc |= pc & ~0x3FFFFFFF;

    int owb = window_base;
    window_base -= n;
    if (window_base == 0)
    {
        //TODO: Underflow exception
        EmuException::die("[Xtensa] Underflow exception!");
    }
    else
        window_start &= ~(1 << owb);

    pc = new_pc;
}

void Xtensa::rfi(int level)
{
    pc = epc[level];
    ps = eps[level];
}
