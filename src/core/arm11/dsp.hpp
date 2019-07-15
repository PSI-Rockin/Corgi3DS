#ifndef DSP_HPP
#define DSP_HPP
#include <cstdint>

class DSP
{
    public:
        DSP();

        void reset();
        void run(int cycles);

        uint16_t read16(uint32_t addr);
        void write16(uint32_t addr, uint16_t value);
};

#endif // DSP_HPP
