#include <cstdio>
#include "../cpu/arm.hpp"
#include "../common/common.hpp"
#include "../timers.hpp"
#include "mpcore_pmr.hpp"

MPCore_PMR::MPCore_PMR(ARM_CPU* appcore, ARM_CPU* syscore, Timers* timers) :
    appcore(appcore), syscore(syscore), timers(timers)
{

}

void MPCore_PMR::reset()
{
    //No interrupt pending
    for (int i = 0; i < 4; i++)
        irq_cause[i] = 0x3FF;

    //Interrupts 0-15 are always enabled
    global_int_mask[0] = 0xFFFF;
}

void MPCore_PMR::assert_hw_irq(int id)
{
    int index = id / 32;
    int bit = id & 0x1F;

    printf("[PMR] Set global IRQ: $%02X\n", id);
    global_int_pending[index] |= 1 << bit;
    set_int_signal(appcore);
    set_int_signal(syscore);
}

void MPCore_PMR::assert_private_irq(int id, int core)
{
    private_int_pending[0] |= 1 << id;
    global_int_pending[0] |= 1 << id;

    printf("[PMR] Set Core%d IRQ: $%02X\n", core, id);

    switch (core)
    {
        case 0:
            set_int_signal(appcore);
            break;
        case 1:
            set_int_signal(syscore);
            break;
        default:
            EmuException::die("[PMR] Unrecognized core ID %d", core);
    }
}

uint8_t MPCore_PMR::read8(int core, uint32_t addr)
{
    printf("[PMR%d] Unrecognized read8 $%08X\n", core, addr);
    return 0;
}

uint32_t MPCore_PMR::read32(int core, uint32_t addr)
{
    if (addr >= 0x17E00600 && addr < 0x17E00A00)
    {
        int timer_id = 0;
        if (addr < 0x17E00700)
            timer_id = core;
        else
            timer_id = (addr - 0x17E00700) / 0x100;

        //Watchdogs
        if (addr & 0x20)
            timer_id += 4;

        switch (addr & 0xFF)
        {
            case 0x00:
            case 0x20:
                return timers->arm11_get_load(timer_id);
            case 0x04:
            case 0x24:
                return timers->arm11_get_counter(timer_id);
            case 0x08:
            case 0x28:
                return timers->arm11_get_control(timer_id);
            case 0x0C:
            case 0x2C:
                return timers->arm11_get_int_status(timer_id);
            default:
                printf("[PMR] Unrecognized timer%d read32 $%08X\n", timer_id, addr);
                return 0;
        }
    }
    if (addr >= 0x17E01100 && addr < 0x17E01120)
        return global_int_mask[(addr / 4) & 0x7];
    if (addr >= 0x17E01200 && addr < 0x17E01220)
        return global_int_pending[(addr / 4) & 0x7];
    if (addr >= 0x17E01280 && addr < 0x17E012A0)
        return global_int_pending[(addr / 4) & 0x7];
    switch (addr)
    {
        case 0x17E0010C:
            return irq_cause[core];
        case 0x17E00118:
            //TODO: This should be the highest priority pending interrupt
            return irq_cause[core];
    }
    printf("[PMR%d] Unrecognized read32 $%08X\n", core, addr);
    return 0;
}

void MPCore_PMR::write8(int core, uint32_t addr, uint8_t value)
{
    printf("[PMR%d] Unrecognized write8 $%08X: $%02X\n", core, addr, value);
}

void MPCore_PMR::write16(int core, uint32_t addr, uint16_t value)
{
    printf("[PMR%d] Unrecognized write16 $%08X: $%04X\n", core, addr, value);
}

void MPCore_PMR::write32(int core, uint32_t addr, uint32_t value)
{
    if (addr >= 0x17E00600 && addr < 0x17E00A00)
    {
        int timer_id = 0;
        if (addr < 0x17E00700)
            timer_id = core;
        else
            timer_id = (addr - 0x17E00700) / 0x100;

        //Watchdogs
        if (addr & 0x20)
            timer_id += 4;

        switch (addr & 0xFF)
        {
            case 0x00:
            case 0x20:
                timers->arm11_set_load(timer_id, value);
                break;
            case 0x04:
            case 0x24:
                timers->arm11_set_counter(timer_id, value);
                break;
            case 0x08:
            case 0x28:
                timers->arm11_set_control(timer_id, value);
                break;
            case 0x0C:
            case 0x2C:
                timers->arm11_set_int_status(timer_id, value);
                break;
            default:
                printf("[PMR] Unrecognized timer%d write32 $%08X: $%08X\n", timer_id, addr, value);
                break;
        }
        return;
    }
    if (addr >= 0x17E01100 && addr < 0x17E01120)
    {
        printf("[PMR%d] Write global int mask set $%08X: $%08X\n", core, addr, value);
        int index = (addr / 4) & 0x7;
        global_int_mask[index] |= value;

        set_int_signal(appcore);
        set_int_signal(syscore);
        return;
    }
    if (addr >= 0x17E01180 && addr < 0x17E011A0)
    {
        printf("[PMR%d] Write global int mask clear $%08X: $%08X\n", core, addr, value);
        int index = (addr / 4) & 0x7;
        global_int_mask[index] &= ~value;
        global_int_mask[0] |= 0xFFFF;

        set_int_signal(appcore);
        set_int_signal(syscore);
        return;
    }
    if (addr >= 0x17E01280 && addr < 0x17E012A0)
    {
        printf("[PMR%d] Clear global int $%08X: $%08X\n", core, addr, value);

        int index = (addr / 4) & 0x7;
        global_int_pending[index] &= ~value;
        if (index == 0)
        {
            for (int i = 0; i < 4; i++)
                private_int_pending[i] &= ~value;
        }
        set_int_signal(appcore);
        set_int_signal(syscore);
        return;
    }
    switch (addr)
    {
        case 0x17E00110:
        {
            //Clear pending interrupt
            printf("[PMR%d] Clear pending int: $%08X\n", core, value);
            int index = (value / 32) & 0x7;
            int bit = value & 0x1F;

            if (index == 0)
                private_int_pending[core] &= ~(1 << bit);
            else
                global_int_pending[index] &= ~(1 << bit);
            if (value == irq_cause[core])
            {
                if (core == 0)
                    set_int_signal(appcore);
                if (core == 1)
                    set_int_signal(syscore);
            }
        }
            return;
        case 0x17E01F00:
            printf("[PMR%d] Send SWI: $%08X\n", core, value);
        {
            uint32_t int_id = value & 0x3FF;
            if (int_id < 32)
            {
                uint8_t target_type = (value >> 24) & 0x3;
                uint8_t target_list = (value >> 16) & 0xF;
                switch (target_type)
                {
                    case 0:
                        if (target_list & 0x1)
                        {
                            global_int_pending[0] |= 1 << int_id;
                            private_int_pending[0] |= 1 << int_id;
                            set_int_signal(appcore, core);
                        }
                        if (target_list & 0x2)
                        {
                            global_int_pending[0] |= 1 << int_id;
                            private_int_pending[1] |= 1 << int_id;
                            set_int_signal(syscore, core);
                        }
                        break;
                    default:
                        EmuException::die("[PMR%d] Unrecognized target type %d\n", core, target_type);
                }
            }
        }
            return;
    }
    printf("[PMR%d] Unrecognized write32 $%08X: $%08X\n", core, addr, value);
}

void MPCore_PMR::set_int_signal(ARM_CPU *core, int id_of_requester)
{
    bool pending = false;
    int id = core->get_id() - 11;
    if (private_int_pending[id] & global_int_mask[0])
    {
        printf("[PMR%d] PENDING INT!\n", id);
        pending = true;
        irq_cause[id] = 0;
        for (int i = 0; i < 32; i++)
        {
            if (private_int_pending[id] & global_int_mask[0] & (1 << i))
            {
                irq_cause[id] += i;
                if (i < 16)
                    irq_cause[id] |= id_of_requester << 10;
                break;
            }
        }
    }
    for (int i = 1; i < 8; i++)
    {
        if (global_int_pending[i] & global_int_mask[i])
        {
            pending = true;
            printf("[PMR%d] PENDING INT!\n", id);
            irq_cause[id] = i * 32;
            for (int j = 0; j < 32; j++)
            {
                if (global_int_pending[i] & global_int_mask[i] & (1 << j))
                {
                    irq_cause[id] += j;
                    break;
                }
            }
            break;
        }
    }
    if (!pending)
        irq_cause[id] = 0x3FF;
    else
        printf("[PMR%d] Int cause: $%04X\n", id, irq_cause[id]);
    core->set_int_signal(pending);
}
