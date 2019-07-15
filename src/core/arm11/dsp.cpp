#include <cstdio>
#include "dsp.hpp"

DSP::DSP()
{

}

void DSP::reset()
{

}

void DSP::run(int cycles)
{

}

uint16_t DSP::read16(uint32_t addr)
{
    uint16_t reg = 0;
    switch (addr)
    {
        default:
            printf("[DSP] Unrecognized read16 $%08X\n", addr);
    }
    return reg;
}

void DSP::write16(uint32_t addr, uint16_t value)
{
    switch (addr)
    {
        default:
            printf("[DSP] Unrecognized write16 $%08X: $%04X\n", addr, value);
    }
}
