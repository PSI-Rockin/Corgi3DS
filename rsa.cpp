#include <cstdio>
#include "rsa.hpp"

RSA::RSA()
{

}

uint32_t RSA::read32(uint32_t addr)
{
    switch (addr)
    {
        default:
            printf("[RSA] Unrecognized read32 $%08X\n", addr);
            return 0;
    }
}

void RSA::write32(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        default:
            printf("[RSA] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}
