#include <cstdio>
#include <cstring>
#include "common/common.hpp"
#include "arm9/interrupt9.hpp"
#include "arm11/mpcore_pmr.hpp"
#include "timers.hpp"
#include "emulator.hpp"

Timers::Timers(Interrupt9* int9, MPCore_PMR* pmr, Emulator* e) : int9(int9), pmr(pmr), e(e)
{

}

void Timers::reset()
{
    memset(arm9_timers, 0, sizeof(arm9_timers));
    memset(arm11_timers, 0, sizeof(arm11_timers));
}

void Timers::run()
{
    for (int i = 0; i < 4; i++)
    {
        if (arm9_timers[i].enabled && !arm9_timers[i].countup)
        {
            arm9_timers[i].clocks++;
            if (arm9_timers[i].clocks >= arm9_timers[i].prescalar)
            {
                arm9_timers[i].counter++;
                arm9_timers[i].clocks -= arm9_timers[i].prescalar;
                if (arm9_timers[i].counter >= 0x10000)
                {
                    handle_overflow(i);
                }
            }
        }
    }
    for (int i = 0; i < 8; i++)
    {
        if (arm11_timers[i].enabled)
        {
            arm11_timers[i].clocks++;
            if (arm11_timers[i].clocks >= arm11_timers[i].prescalar)
            {
                arm11_timers[i].counter--;
                arm11_timers[i].clocks -= arm11_timers[i].prescalar;
                if (arm11_timers[i].counter == 0)
                {
                    if (arm11_timers[i].auto_reload)
                        arm11_timers[i].counter = arm11_timers[i].load;
                    else
                        arm11_timers[i].enabled = false;

                    arm11_timers[i].int_flag = true;
                    if (arm11_timers[i].int_enabled)
                    {
                        pmr->assert_private_irq(29 + (i / 4), i & 0x3);
                    }
                }
            }
        }
    }
}

void Timers::handle_overflow(int index)
{
    arm9_timers[index].counter -= 0x10000;
    printf("[Timer9] Overflow on timer %d!\n", index);
    if (arm9_timers[index].overflow_irq)
        int9->assert_irq(8 + index);

    arm9_timers[index].counter = arm9_timers[index].reload;

    if (index != 3)
    {
        if (arm9_timers[index + 1].countup && arm9_timers[index + 1].enabled)
        {
            arm9_timers[index + 1].counter++;
            if (arm9_timers[index + 1].counter >= 0x10000)
                handle_overflow(index + 1);
        }
    }
}

uint16_t Timers::arm9_read16(uint32_t addr)
{
    switch (addr)
    {
        case 0x10003000:
            return arm9_timers[0].counter;
        case 0x10003002:
            return get_control(0);
        case 0x10003004:
            return arm9_timers[1].counter;
        case 0x10003006:
            return get_control(1);
        case 0x10003008:
            return arm9_timers[2].counter;
        case 0x1000300A:
            return get_control(2);
        case 0x1000300C:
            return arm9_timers[3].counter;
        case 0x1000300E:
            return get_control(3);
    }
    printf("[Timer9] Unrecognized read16 $%08X\n", addr);
    return 0;
}

void Timers::arm9_write16(uint32_t addr, uint16_t value)
{
    switch (addr)
    {
        case 0x10003000:
            set_counter(0, value);
            return;
        case 0x10003002:
            set_control(0, value);
            return;
        case 0x10003004:
            set_counter(1, value);
            return;
        case 0x10003006:
            set_control(1, value);
            return;
        case 0x10003008:
            set_counter(2, value);
            return;
        case 0x1000300A:
            set_control(2, value);
            return;
        case 0x1000300C:
            set_counter(3, value);
            return;
        case 0x1000300E:
            set_control(3, value);
            return;
    }
    printf("[Timer9] Unrecognized write16 $%08X: $%04X\n", addr, value);
}

uint16_t Timers::get_control(int index)
{
    uint16_t reg = 0;
    switch (arm9_timers[index].prescalar)
    {
        case 64 * 2:
            reg = 1;
            break;
        case 256 * 2:
            reg = 2;
            break;
        case 1024 * 2:
            reg = 3;
            break;
    }
    reg |= arm9_timers[index].countup << 2;
    reg |= arm9_timers[index].overflow_irq << 6;
    reg |= arm9_timers[index].enabled << 7;
    printf("[Timer9] Get timer%d control: $%04X\n", index, reg);
    return reg;
}

void Timers::set_counter(int index, uint16_t value)
{
    printf("[Timer9] Set timer%d reload: $%04X\n", index, value);
    arm9_timers[index].reload = value;
}

void Timers::set_control(int index, uint16_t value)
{
    printf("[Timer9] Set timer%d ctrl: $%04X\n", index, value);
    const static int prescalar_values[] = {1, 64, 256, 1024};
    arm9_timers[index].clocks = 0;

    //Multiply prescalar by 2 because timers run at half the speed of the ARM9
    arm9_timers[index].prescalar = prescalar_values[value & 0x3] << 1;
    arm9_timers[index].countup = value & (1 << 2);
    arm9_timers[index].overflow_irq = value & (1 << 6);

    bool old_enabled = arm9_timers[index].enabled;
    arm9_timers[index].enabled = value & (1 << 7);

    if (!old_enabled && arm9_timers[index].enabled)
    {
        arm9_timers[index].clocks = 0;
        arm9_timers[index].counter = arm9_timers[index].reload;
    }
}

uint32_t Timers::arm11_get_load(int id)
{
    return arm11_timers[id].load;
}

uint32_t Timers::arm11_get_counter(int id)
{
    printf("[Timers] Read ARM11 timer%d counter: $%08X\n", id, arm11_timers[id].counter);
    return arm11_timers[id].counter;
}

uint32_t Timers::arm11_get_control(int id)
{
    uint32_t reg = 0;
    reg = arm11_timers[id].enabled;
    reg |= arm11_timers[id].auto_reload << 1;
    reg |= arm11_timers[id].int_enabled << 2;
    reg |= arm11_timers[id].watchdog_mode << 3;
    reg |= ((arm11_timers[id].prescalar / 2) - 1) << 8;
    return reg;
}

uint32_t Timers::arm11_get_int_status(int id)
{
    return arm11_timers[id].int_flag;
}

void Timers::arm11_set_load(int id, uint32_t value)
{
    printf("[Timers] Set ARM11 timer%d load: $%08X\n", id, value);
    arm11_timers[id].load = value;
    arm11_timers[id].counter = value;
}

void Timers::arm11_set_counter(int id, uint32_t value)
{
    printf("[Timers] Set ARM11 timer%d counter: $%08X\n", id, value);
    arm11_timers[id].counter = value;
}

void Timers::arm11_set_control(int id, uint32_t value)
{
    printf("[Timers] Set ARM11 timer%d control: $%08X\n", id, value);
    arm11_timers[id].enabled = value & 0x1;
    arm11_timers[id].auto_reload = (value >> 1) & 0x1;
    arm11_timers[id].int_enabled = (value >> 2) & 0x1;
    arm11_timers[id].prescalar = (value >> 8) & 0xFF;
    arm11_timers[id].prescalar++;
    arm11_timers[id].prescalar <<= 1;
}

void Timers::arm11_set_int_status(int id, uint32_t value)
{
    printf("[Timers] Set ARM11 timer%d status: $%08X\n", id, value);
    arm11_timers[id].int_flag &= ~(value & 0x1);
}
