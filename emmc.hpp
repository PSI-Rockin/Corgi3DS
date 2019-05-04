#ifndef EMMC_HPP
#define EMMC_HPP
#include <cstdint>

class EMMC
{
    public:
        EMMC();

        uint16_t read16(uint32_t addr);
        void write16(uint32_t addr, uint16_t value);
};

#endif // EMMC_HPP
