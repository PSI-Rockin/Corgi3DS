#ifndef DMA9_HPP
#define DMA9_HPP
#include <cstdint>

class DMA9
{
    public:
        DMA9();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // DMA9_HPP
