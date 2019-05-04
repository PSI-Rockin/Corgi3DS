#ifndef RSA_HPP
#define RSA_HPP
#include <cstdint>

class RSA
{
    public:
        RSA();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // RSA_HPP
