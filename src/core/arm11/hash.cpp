#include "../common/common.hpp"
#include "hash.hpp"

#define printf(fmt,...)(0)

HASH::HASH()
{

}

uint32_t HASH::read32(uint32_t addr)
{
    uint32_t reg = 0;
    if (addr >= 0x10101040 && addr < 0x10101060)
    {
        int index = (addr / 4) & 0x7;
        uint32_t value = *(uint32_t*)&eng.hash[index];
        if (eng.SHA_CNT.out_big_endian)
            value = bswp32(value);
        printf("[HASH] Read32 hash $%08X: $%08X\n", addr, value);
        return value;
    }
    switch (addr)
    {
        case 0x10101000:
            reg |= eng.SHA_CNT.in_dma_enable << 2;
            reg |= eng.SHA_CNT.out_big_endian << 3;
            reg |= eng.SHA_CNT.mode << 4;
            reg |= eng.SHA_CNT.out_dma_enable << 10;
            printf("[HASH] Read32 HASH_CNT: $%08X\n", reg);
            break;
        case 0x10101004:
            reg = eng.message_len << 2;
            break;
        default:
            printf("[HASH] Unrecognized read32 $%08X\n", addr);
    }
    return reg;
}

void HASH::write32(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        case 0x10101000:
            printf("[HASH] Write32 HASH_CNT: $%08X\n", value);
            eng.SHA_CNT.busy = value & 0x1;
            eng.SHA_CNT.in_dma_enable = value & (1 << 2);
            eng.SHA_CNT.out_big_endian = value & (1 << 3);
            eng.SHA_CNT.mode = (value >> 4) & 0x3;
            eng.SHA_CNT.out_dma_enable = value & (1 << 10);

            /*if (!eng.SHA_CNT.in_dma_enable)
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
            }*/

            if (value & 0x1)
                eng.reset_hash();
            if (value & (1 << 1))
                eng.do_hash(true);
            break;
        default:
            printf("[HASH] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}

void HASH::write_fifo(uint32_t value)
{
    if (eng.in_fifo.size() == 0)
    {
        //dma9->clear_ndma_req(NDMA_SHA_OUT);
        //dma9->clear_xdma_req(XDMA_SHA);
        std::queue<uint32_t> empty;
        eng.read_fifo.swap(empty);
    }
    printf("[HASH] Write FIFO: $%08X\n", value);
    eng.in_fifo.push(value);
    eng.read_fifo.push(value);
    eng.message_len++;
    if (eng.in_fifo.size() == 16)
    {
        /*if (eng.SHA_CNT.out_dma_enable)
        {
            dma9->set_xdma_req(XDMA_SHA);
            dma9->set_ndma_req(NDMA_SHA_OUT);
        }
        dma9->clear_ndma_req(NDMA_AES2);*/

        eng.do_hash(false);
        eng.SHA_CNT.fifo_enable = true;
    }
    else
        eng.SHA_CNT.fifo_enable = false;
}
