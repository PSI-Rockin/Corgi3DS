#include <cstdio>
#include "arm.hpp"
#include "interrupt9.hpp"

Interrupt9::Interrupt9(ARM_CPU* arm9) : arm9(arm9)
{

}

uint32_t Interrupt9::read_ie()
{
    return IE;
}

uint32_t Interrupt9::read_if()
{
    printf("[Int9] Read IF\n");
    return IF;
}

void Interrupt9::write_ie(uint32_t value)
{
    printf("[Int9] IE: $%08X\n", value);
    IE = value;
}

void Interrupt9::write_if(uint32_t value)
{
    printf("[Int9] IF: $%08X\n", value);
    IF &= ~value;
}

void Interrupt9::assert_irq(int id)
{
    IF |= 1 << id;
    if (IE & IF)
        arm9->int_check();
}
