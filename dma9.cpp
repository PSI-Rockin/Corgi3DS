#include <cstdio>
#include "dma9.hpp"

DMA9::DMA9()
{

}

uint32_t DMA9::read32(uint32_t addr)
{
    printf("[DMA9] Unrecognized read32 $%08X\n", addr);
    return 0;
}

void DMA9::write32(uint32_t addr, uint32_t value)
{
    printf("[DMA9] Unrecognized write32 $%08X: $%08X\n", addr, value);
}
