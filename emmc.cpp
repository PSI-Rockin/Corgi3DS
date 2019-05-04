#include <cstdio>
#include "emmc.hpp"

EMMC::EMMC()
{

}

uint16_t EMMC::read16(uint32_t addr)
{
    /*switch (addr)
    {
        case 0x1000601C:
            return 0x4;
    }*/
    printf("[EMMC] Unrecognized read16 $%08X\n", addr);
    return 0;
}

void EMMC::write16(uint32_t addr, uint16_t value)
{
    printf("[EMMC] Unrecognized write16 $%08X: $%04X\n", addr, value);
}
