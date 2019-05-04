#include <cstdio>
#include "arm.hpp"
#include "mpcore_pmr.hpp"

MPCore_PMR::MPCore_PMR(ARM_CPU* syscore) : syscore(syscore)
{

}

void MPCore_PMR::reset()
{
    //No interrupt pending
    irq_cause = 0x3FF;
}

void MPCore_PMR::assert_hw_irq(int id)
{
    int index = id / 32;
    int bit = id & 0x1F;

    hw_int_pending[index] |= 1 << bit;
    if (hw_int_pending[index] & hw_int_mask[index])
        syscore->int_check();
}

uint8_t MPCore_PMR::read8(uint32_t addr)
{
    printf("[PMR] Unrecognized read8 $%08X\n", addr);
    return 0;
}

uint32_t MPCore_PMR::read32(uint32_t addr)
{
    if (addr >= 0x17E01100 && addr < 0x17E01120)
        return hw_int_mask[(addr / 4) & 0x7];
    if (addr >= 0x17E01200 && addr < 0x17E01220)
        return hw_int_pending[(addr / 4) & 0x7];
    switch (addr)
    {
        case 0x17E0010C:
            return irq_cause;
    }
    printf("[PMR] Unrecognized read32 $%08X\n", addr);
    return 0;
}

void MPCore_PMR::write8(uint32_t addr, uint8_t value)
{
    printf("[PMR] Unrecognized write8 $%08X: $%02X\n", addr, value);
}

void MPCore_PMR::write16(uint32_t addr, uint16_t value)
{
    printf("[PMR] Unrecognized write16 $%08X: $%04X\n", addr, value);
}

void MPCore_PMR::write32(uint32_t addr, uint32_t value)
{
    if (addr >= 0x17E01100 && addr < 0x17E01120)
    {
        hw_int_mask[(addr / 4) & 0x7] = value;
        return;
    }
    if (addr >= 0x17E01200 && addr < 0x17E01220)
    {
        hw_int_pending[(addr / 4) & 0x7] &= ~value;
        return;
    }
    printf("[PMR] Unrecognized write32 $%08X: $%08X\n", addr, value);
}
