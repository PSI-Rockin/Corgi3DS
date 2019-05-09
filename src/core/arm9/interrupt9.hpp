#ifndef INTERRUPT9_HPP
#define INTERRUPT9_HPP
#include <cstdint>

class ARM_CPU;

class Interrupt9
{
    private:
        ARM_CPU* arm9;
        uint32_t IE, IF;
    public:
        Interrupt9(ARM_CPU* arm9);

        uint32_t read_ie();
        uint32_t read_if();
        void write_ie(uint32_t value);
        void write_if(uint32_t value);

        void assert_irq(int id);
};

#endif // INTERRUPT9_HPP
