#include <cstdio>
#include <cstdlib>
#include "../common/common.hpp"
#include "dma9.hpp"
#include "sha.hpp"

SHA::SHA(DMA9* dma9) : dma9(dma9)
{

}

void SHA::reset()
{
    eng.SHA_CNT.busy = false;
    eng.SHA_CNT.final_round = false;
    eng.message_len = 0;
}

uint8_t SHA::read_hash(uint32_t addr)
{
    int index = (addr / 4) & 0x7;
    int offset = addr & 0x3;
    if (eng.SHA_CNT.out_big_endian)
        offset = 3 - offset;
    printf("[SHA] Read hash: $%08X $%02X\n", addr, (eng.hash[index] >> (offset * 8)) & 0xFF);
    return (eng.hash[index] >> (offset * 8)) & 0xFF;
}

uint32_t SHA::read32(uint32_t addr)
{
    uint32_t reg = 0;
    if (addr >= 0x1000A040 && addr < 0x1000A080)
    {
        int index = (addr / 4) & 0x7;
        uint32_t value = *(uint32_t*)&eng.hash[index];
        if (eng.SHA_CNT.out_big_endian)
            value = bswp32(value);
        printf("[SHA] Read32 hash $%08X: $%08X\n", addr, value);
        return value;
    }
    if (addr >= 0x1000A080 && addr < 0x1000A0C0)
    {
        uint32_t value = eng.read_fifo.front();
        //printf("[SHA] Read FIFO: $%08X\n", value);
        eng.read_fifo.pop();
        if (!eng.read_fifo.size())
        {
            eng.SHA_CNT.fifo_enable = false;
            dma9->clear_xdma_req(XDMA_SHA);
            dma9->clear_ndma_req(NDMA_SHA_OUT);
            dma9->set_ndma_req(NDMA_AES2);
        }
        return value;
    }
    switch (addr)
    {
        case 0x1000A000:
            //reg |= SHA_CNT.busy;
            //reg |= SHA_CNT.final_round << 1;
            reg |= eng.SHA_CNT.in_dma_enable << 2;
            reg |= eng.SHA_CNT.out_big_endian << 3;
            reg |= eng.SHA_CNT.mode << 4;
            reg |= eng.SHA_CNT.fifo_enable << 9;
            reg |= eng.SHA_CNT.out_dma_enable << 10;
            //printf("[SHA] Read32 SHA_CNT: $%08X\n", reg);
            break;
        case 0x1000A004:
            reg = eng.message_len * 4;
            break;
        default:
            printf("[SHA] Unrecognized read32 $%08X\n", addr);
    }
    return reg;
}

void SHA::write32(uint32_t addr, uint32_t value)
{
    if (addr >= 0x1000A040 && addr < 0x1000A060)
    {
        int index = (addr / 4) & 0x7;
        *(uint32_t*)&eng.hash[index] = bswp32(value);
        printf("[SHA] Write32 hash $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x1000A080 && addr < 0x1000A0C0)
    {
        write_fifo(value);
        return;
    }
    switch (addr)
    {
        case 0x1000A000:
            printf("[SHA] Write32 SHA_CNT: $%08X\n", value);
            eng.SHA_CNT.busy = value & 0x1;
            eng.SHA_CNT.in_dma_enable = value & (1 << 2);
            eng.SHA_CNT.out_big_endian = value & (1 << 3);
            eng.SHA_CNT.mode = (value >> 4) & 0x3;
            eng.SHA_CNT.out_dma_enable = value & (1 << 10);

            if (!eng.SHA_CNT.in_dma_enable)
                dma9->clear_ndma_req(NDMA_SHA_IN);
            else if (eng.in_fifo.size() < 16)
                dma9->set_ndma_req(NDMA_SHA_IN);

            if (!eng.SHA_CNT.out_dma_enable)
            {
                dma9->clear_ndma_req(NDMA_SHA_OUT);
                dma9->clear_xdma_req(XDMA_SHA);
            }
            else if (eng.in_fifo.size() == 16)
            {
                dma9->set_ndma_req(NDMA_SHA_OUT);
                dma9->set_xdma_req(XDMA_SHA);
            }

            if (value & 0x1)
                eng.reset_hash();
            if (value & (1 << 1))
                eng.do_hash(true);
            return;
        case 0x1000A004:
            printf("[SHA] Write message len: $%08X\n", value);
            eng.message_len = value / 4;
            return;
    }
    printf("[SHA] Unrecognized write32 $%08X: $%08X\n", addr, value);
}

void SHA::write_fifo(uint32_t value)
{
    if (eng.in_fifo.size() == 0)
    {
        dma9->clear_ndma_req(NDMA_SHA_OUT);
        dma9->clear_xdma_req(XDMA_SHA);
        std::queue<uint32_t> empty;
        eng.read_fifo.swap(empty);
    }
    //printf("[SHA] Write FIFO: $%08X\n", value);
    eng.in_fifo.push(value);
    eng.read_fifo.push(value);
    eng.message_len++;
    if (eng.in_fifo.size() == 16)
    {
        if (eng.SHA_CNT.out_dma_enable)
        {
            dma9->set_xdma_req(XDMA_SHA);
            dma9->set_ndma_req(NDMA_SHA_OUT);
        }
        dma9->clear_ndma_req(NDMA_AES2);

        eng.do_hash(false);
        eng.SHA_CNT.fifo_enable = true;
    }
    else
        eng.SHA_CNT.fifo_enable = false;
}
