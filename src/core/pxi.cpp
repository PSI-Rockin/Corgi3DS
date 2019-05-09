#include <cstdio>
#include "arm11/mpcore_pmr.hpp"
#include "arm9/interrupt9.hpp"
#include "pxi.hpp"

PXI::PXI(MPCore_PMR* mpcore, Interrupt9* int9) : mpcore(mpcore), int9(int9)
{

}

uint32_t PXI::read_sync9()
{
    uint32_t reg = sync9.recv_data;
    reg |= sync11.recv_data << 8;
    reg |= sync9.local_irq << 31;
    return reg;
}

uint32_t PXI::read_sync11()
{
    uint32_t reg = sync11.recv_data;
    reg |= sync9.recv_data << 8;
    reg |= sync11.local_irq << 31;
    return reg;
}

uint16_t PXI::read_cnt9()
{
    uint16_t reg = 0;
    reg |= recv11.size() == 0;
    reg |= (recv11.size() == 16) << 1;
    reg |= cnt9.send_empty_irq << 2;
    reg |= (recv9.size() == 0) << 8;
    reg |= (recv9.size() == 16) << 9;
    reg |= cnt9.recv_not_empty_irq << 10;
    reg |= cnt9.error << 14;
    reg |= cnt9.enable << 15;
    printf("[PXI] Read CNT9: $%04X\n", reg);
    return reg;
}

uint16_t PXI::read_cnt11()
{
    uint16_t reg = 0;
    reg |= recv9.size() == 0;
    reg |= (recv9.size() == 16) << 1;
    reg |= cnt11.send_empty_irq << 2;
    reg |= (recv11.size() == 0) << 8;
    reg |= (recv11.size() == 16) << 9;
    reg |= cnt11.recv_not_empty_irq << 10;
    reg |= cnt11.error << 14;
    reg |= cnt11.enable << 15;
    printf("[PXI] Read CNT11: $%04X\n", reg);
    return reg;
}

void PXI::write_sync9(uint32_t value)
{
    printf("[PXI] Write sync9: $%08X\n", value);
    sync11.recv_data = (value >> 8) & 0xFF;
    sync9.local_irq = value & (1 << 31);

    if ((value & (1 << 29)) && sync11.local_irq)
        mpcore->assert_hw_irq(0x50);
}

void PXI::write_sync11(uint32_t value)
{
    printf("[PXI] Write sync11: $%08X\n", value);
    sync9.recv_data = (value >> 8) & 0xFF;
    sync11.local_irq = value & (1 << 31);

    if ((value & (1 << 30)) && sync9.local_irq)
        int9->assert_irq(12);
}

void PXI::write_cnt9(uint16_t value)
{
    printf("[PXI] Write CNT9: $%04X\n", value);

    cnt9.send_empty_irq = value & (1 << 2);
    cnt9.recv_not_empty_irq = value & (1 << 10);
    cnt9.error &= ~(value & (1 << 14));
    cnt9.enable = value & (1 << 15);

    //Clear send FIFO
    if (value & (1 << 3))
    {
        std::queue<uint32_t> empty;
        recv11.swap(empty);
    }
}

void PXI::write_cnt11(uint16_t value)
{
    printf("[PXI] Write CNT11: $%04X\n", value);

    cnt11.send_empty_irq = value & (1 << 2);
    cnt11.recv_not_empty_irq = value & (1 << 10);
    cnt11.error &= ~(value & (1 << 14));
    cnt11.enable = value & (1 << 15);

    //Clear send FIFO
    if (value & (1 << 3))
    {
        std::queue<uint32_t> empty;
        recv9.swap(empty);
    }
}

uint32_t PXI::read_msg9()
{
    if (recv9.size())
    {
        last_recv9 = recv9.front();
        recv9.pop();
    }
    return last_recv9;
}

uint32_t PXI::read_msg11()
{
    if (recv11.size())
    {
        last_recv11 = recv11.front();
        recv11.pop();
    }
    return last_recv11;
}

void PXI::send_to_9(uint32_t value)
{
    recv9.push(value);
}

void PXI::send_to_11(uint32_t value)
{
    printf("[PXI] Send to 11: $%08X\n", value);
    recv11.push(value);
}
