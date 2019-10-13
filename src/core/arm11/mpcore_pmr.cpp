#include <cstdio>
#include <cstring>
#include "../cpu/arm.hpp"
#include "../common/common.hpp"
#include "../timers.hpp"
#include "mpcore_pmr.hpp"

MPCore_PMR::MPCore_PMR(ARM_CPU arm11[4], Timers* timers) :
    arm11(arm11), timers(timers)
{

}

void MPCore_PMR::reset(int core_count)
{
    this->core_count = core_count;
    //No interrupt pending
    for (int i = 0; i < 4; i++)
    {
        local_irq_ctrl[i].enabled = false;
        local_irq_ctrl[i].highest_priority_pending = SPURIOUS_INT;
        local_irq_ctrl[i].irq_cause = SPURIOUS_INT;
        local_irq_ctrl[i].cur_active_irq = SPURIOUS_INT;
        local_irq_ctrl[i].preemption_mask = 0x3;
        local_irq_ctrl[i].priority_mask = 0xF;
    }

    memset(private_int_pending, 0, sizeof(private_int_pending));
    memset(private_int_active, 0, sizeof(private_int_active));
    memset(private_int_priority, 0, sizeof(private_int_priority));


    //Interrupts 0-15 are always enabled
    global_int_mask[0] = 0xFFFF;
}

void MPCore_PMR::assert_hw_irq(int id)
{
    //printf("[PMR] Set global IRQ: $%02X\n", id);

    uint8_t cpu_targets = global_int_targets[id - 32];

    for (int i = 0; i < core_count; i++)
    {
        //Only set the interrupt to pending if the core is in the target list
        if (cpu_targets & (1 << i))
            set_pending_irq(i, id);
    }
}

uint8_t MPCore_PMR::get_int_priority(int core, int int_id)
{
    if (int_id < 32)
        return private_int_priority[core][int_id];
    return global_int_priority[int_id - 32];
}

uint32_t MPCore_PMR::find_highest_priority_pending(int core)
{
    //Work backwards. Lower IDs have higher priority than higher IDs, assuming they have the same priority.

    uint32_t pending = SPURIOUS_INT;
    int last_priority = 0xF;
    for (int i = 127; i >= 0; i--)
    {
        int index = i / 32;
        int bit = i & 0x1F;

        if (private_int_pending[core][index] & (1 << bit))
        {
            if (global_int_mask[index] & (1 << bit))
            {
                uint8_t priority = get_int_priority(core, i);
                if (priority <= last_priority)
                {
                    pending = i;
                    last_priority = priority;

                    //Get ID of the core that requested an SWI
                    if (i < 16)
                        pending |= private_int_requestor[core][i] << 10;
                }
            }
        }
    }
    return pending;
}

void MPCore_PMR::check_if_can_assert_irq(int core)
{
    set_int_signal(core, false);

    local_irq_ctrl[core].highest_priority_pending = find_highest_priority_pending(core);

    int int_id = local_irq_ctrl[core].highest_priority_pending & 0x3FF;

    //No interrupts pending - return early
    if (int_id == SPURIOUS_INT)
        return;

    uint8_t priority = get_int_priority(core, int_id);
    if (priority < local_irq_ctrl[core].priority_mask)
    {
        //If there is an active IRQ for this core, we must check if we can preempt it
        uint32_t cur_active_irq = local_irq_ctrl[core].cur_active_irq & 0x3FF;
        if (cur_active_irq != SPURIOUS_INT)
        {
            uint8_t active_priority = get_int_priority(core, cur_active_irq);
            switch (local_irq_ctrl[core].preemption_mask)
            {
                case 0x4: //Bits 3-1 only.
                    priority &= ~0x1;
                    active_priority &= ~0x1;
                    break;
                case 0x5: //Bits 3-2 only.
                    priority &= ~0x3;
                    active_priority &= ~0x3;
                    break;
                case 0x6: //Bit 3 only.
                    priority &= ~0x7;
                    active_priority &= ~0x7;
                    break;
                case 0x7: //No preemption allowed.
                    return;
                default: //All bits used
                    break;
            }

            //Preemption check
            if (priority < active_priority)
                EmuException::die("[PMR%d] PREEMPTION!", core);
            else
                return;
        }

        //Interrupt has occurred. Send signal to the CPU and set the IRQ cause.
        local_irq_ctrl[core].irq_cause = local_irq_ctrl[core].highest_priority_pending;

        set_int_signal(core, true);
    }
}

void MPCore_PMR::set_pending_irq(int core, int int_id, int id_of_requester)
{
    printf("[PMR%d] Set pending int%d\n", core, int_id);
    int index = int_id / 32;
    int bit = int_id & 0x1F;
    private_int_pending[core][index] |= 1 << bit;

    if (int_id < 16)
        private_int_requestor[core][int_id] = id_of_requester;

    check_if_can_assert_irq(core);
}

uint8_t MPCore_PMR::read8(int core, uint32_t addr)
{
    if (addr >= 0x17E01800 && addr < 0x17E01880)
    {
        if (addr >= 0x17E01820)
            return global_int_targets[addr - 0x17E01820];
        return 0;
    }
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
                return timers->arm11_get_counter(timer_id, arm11[core].get_cycles_ran());
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
    if ((addr >= 0x17E01200 && addr < 0x17E01220) || (addr >= 0x17E01280 && addr < 0x17E012A0))
    {
        bool is_aliased = ((addr / 4) & 0x7) == 0;

        if (is_aliased)
            return private_int_pending[core][0];

        uint32_t global_pending = 0;

        for (int i = 0; i < 4; i++)
            global_pending |= private_int_pending[i][(addr / 4) & 0x7];
        return global_pending;
    }
    if (addr >= 0x17E01400 && addr < 0x17E01480)
    {
        uint32_t value = 0;
        for (int i = 0; i < 4; i++)
            value |= (get_int_priority(core, addr - 0x17E01400) << 4) << (i * 8);
        return value;
    }
    if (addr >= 0x17E01800 && addr < 0x17E01880)
    {
        if (addr >= 0x17E01820)
            return *(uint32_t*)&global_int_targets[addr - 0x17E01820];
        if (addr == 0x17E0181C)
        {
            //29-31 are always 1. 0-28 are always 0 (as they are SWIs)
            switch (core)
            {
                case 0:
                    return 0x01010100;
                case 1:
                    return 0x02020200;
                case 2:
                    return 0x04040400;
                case 3:
                    return 0x08080800;
            }
        }
        return 0;
    }
    switch (addr)
    {
        case 0x17E00004:
            printf("[PMR%d] Read SCU_CONFIG\n", core);
            return (core_count - 1) | (0xF << 4);
        case 0x17E0010C:
        {
            //Reading returns the cause of the IRQ and acknowledges it
            uint32_t cause = local_irq_ctrl[core].irq_cause;
            printf("[PMR%d] Acknowledge pending IRQ: $%08X\n", core, cause);

            int int_id = cause & 0x3FF;
            private_int_pending[core][int_id / 32] &= ~(1 << (int_id & 0x1F));
            local_irq_ctrl[core].irq_cause = SPURIOUS_INT;
            local_irq_ctrl[core].cur_active_irq = cause;
            check_if_can_assert_irq(core);
            return cause;
        }
        case 0x17E00118:
            return local_irq_ctrl[core].highest_priority_pending;
        case 0x17E01004:
            return ((core_count - 1) << 5) | 0x3;
    }
    printf("[PMR%d] Unrecognized read32 $%08X\n", core, addr);
    return 0;
}

void MPCore_PMR::write8(int core, uint32_t addr, uint8_t value)
{
    if (addr >= 0x17E01400 && addr < 0x17E01480)
    {
        //printf("[PMR%d] Write8 int%d priority: $%02X\n", core, addr - 0x17E01400, value);
        //Interrupt IDs 0-31 are aliased for each CPU
        if (addr < 0x17E01420)
            private_int_priority[core][addr - 0x17E01400] = value >> 4;
        else
            global_int_priority[addr - 0x17E01420] = value >> 4;

        for (int i = 0; i < core_count; i++)
            check_if_can_assert_irq(i);
        return;
    }
    if (addr >= 0x17E01820 && addr < 0x17E01880)
    {
        printf("[PMR%d] Write8 int%d target: $%02X\n", core, addr - 0x17E01800, value);
        global_int_targets[addr - 0x17E01820] = value & 0xF;
        return;
    }
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
        //printf("[PMR%d] Write global int mask set $%08X: $%08X\n", core, addr, value);
        int index = (addr / 4) & 0x7;
        global_int_mask[index] |= value;

        for (int i = 0; i < core_count; i++)
            check_if_can_assert_irq(i);
        return;
    }
    if (addr >= 0x17E01180 && addr < 0x17E011A0)
    {
        //printf("[PMR%d] Write global int mask clear $%08X: $%08X\n", core, addr, value);
        int index = (addr / 4) & 0x7;
        global_int_mask[index] &= ~value;
        global_int_mask[0] |= 0xFFFF;

        for (int i = 0; i < core_count; i++)
            check_if_can_assert_irq(i);
        return;
    }
    if (addr >= 0x17E01280 && addr < 0x17E012A0)
    {
        printf("[PMR%d] Clear global int $%08X: $%08X\n", core, addr, value);

        int index = (addr / 4) & 0x7;
        bool is_aliased = index == 0;
        if (is_aliased)
            private_int_pending[core][0] &= ~value;
        else
        {
            for (int i = 0; i < 4; i++)
                private_int_pending[i][index] &= ~value;
        }

        for (int i = 0; i < core_count; i++)
            check_if_can_assert_irq(i);
        return;
    }
    if (addr >= 0x17E01820 && addr < 0x17E01880)
    {
        value &= 0x0F0F0F0F;
        *(uint32_t*)&global_int_targets[addr - 0x17E01820] = value;
        return;
    }
    switch (addr)
    {
        case 0x17E00100:
            printf("[PMR%d] Set local IRQ enable: $%08X\n", core, value);
            local_irq_ctrl[core].enabled = value & 0x1;
            return;
        case 0x17E00104:
            printf("[PMR%d] Set local priority mask: $%08X\n", core, value);
            local_irq_ctrl[core].priority_mask = (value >> 4) & 0xF;
            return;
        case 0x17E00108:
            printf("[PMR%d] Set local preemption mask: $%08X\n", core, value);
            local_irq_ctrl[core].preemption_mask = value & 0x7;
            if (local_irq_ctrl[core].preemption_mask < 0x3)
                local_irq_ctrl[core].preemption_mask = 0x3;
            return;
        case 0x17E00110:
        {
            //Clear active interrupt
            //printf("[PMR%d] Clear active int: $%08X\n", core, value);

            local_irq_ctrl[core].cur_active_irq = SPURIOUS_INT;
            check_if_can_assert_irq(core);
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
                        //Send to CPUs within target list
                        for (int i = 0; i < core_count; i++)
                        {
                            if (target_list & (1 << i))
                                set_pending_irq(i, int_id, core);
                        }
                        break;
                    case 1:
                        //Send to all CPUs except the sender
                        for (int i = 0; i < core_count; i++)
                        {
                            if (core != i)
                                set_pending_irq(i, int_id, core);
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

void MPCore_PMR::set_int_signal(int core, bool irq)
{
    arm11[core].set_int_signal(irq);
}
