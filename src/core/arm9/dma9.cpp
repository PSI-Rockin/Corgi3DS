#include <cstdio>
#include <cstring>
#include "dma9.hpp"
#include "../common/common.hpp"
#include "../emulator.hpp"

DMA9::DMA9(Emulator* e, Interrupt9* int9, Scheduler* scheduler) : e(e), int9(int9), scheduler(scheduler)
{

}

void DMA9::reset()
{
    memset(pending_ndma_reqs, 0, sizeof(pending_ndma_reqs));
    memset(ndma_chan, 0, sizeof(ndma_chan));

    xdma.reset();

    global_ndma_ctrl = 0;

    xdma.set_mem_read8_func([this](uint32_t addr) -> uint8_t{return e->arm9_read8(addr);});
    xdma.set_mem_read32_func([this](uint32_t addr) -> uint32_t{return e->arm9_read32(addr);});
    xdma.set_mem_write8_func([this](uint32_t addr, uint8_t value) {e->arm9_write8(addr, value);});
    xdma.set_mem_write32_func([this](uint32_t addr, uint32_t value) {e->arm9_write32(addr, value);});
    xdma.set_send_interrupt([this](int chan) {int9->assert_irq(28);});
}

void DMA9::process_ndma_reqs()
{
    for (int i = 0; i < 8; i++)
    {
        if (ndma_chan[i].busy && pending_ndma_reqs[ndma_chan[i].startup_mode])
        {
            run_ndma(i);
            while (i < 7 && ndma_chan[i + 1].startup_mode == NDMA_AES2)
            {
                i++;
                run_ndma(i);
            }
        }
    }
}

void DMA9::run_xdma()
{
    xdma.run();
}

void DMA9::try_ndma_transfer(NDMA_Request req)
{
    scheduler->add_event(
                [this](uint64_t param) { this->try_ndma_transfer_event((NDMA_Request)param); },
                1,
                (uint64_t)req
                );
}

void DMA9::try_ndma_transfer_event(NDMA_Request req)
{
    //printf("[DMA9] Try NDMA req%d\n", req);
    for (int i = 0; i < 8; i++)
    {
        if (ndma_chan[i].busy && ndma_chan[i].startup_mode == req)
        {
            //printf("Success\n");
            pending_ndma_reqs[req] = true;
            return;
        }
    }
    //printf("Fail\n");
    pending_ndma_reqs[req] = false;
}

void DMA9::set_ndma_req(NDMA_Request req)
{
    //printf("[DMA9] Set NDMA req%d\n", req);
    pending_ndma_reqs[req] = true;
}

void DMA9::clear_ndma_req(NDMA_Request req)
{
    //printf("[DMA9] Clear NDMA req%d\n", req);
    pending_ndma_reqs[req] = false;
}

void DMA9::set_xdma_req(XDMA_Request req)
{
    xdma.set_pending((int)req);
}

void DMA9::clear_xdma_req(XDMA_Request req)
{
    xdma.clear_pending((int)req);
}

void DMA9::run_ndma(int chan)
{
    int src_multiplier = 0;
    switch (ndma_chan[chan].src_update_method)
    {
        case 0:
            //Increment
            src_multiplier = 4;
            break;
        case 1:
            //Decrement
            src_multiplier = -4;
            break;
        case 2:
            //Fixed
            src_multiplier = 0;
            break;
        case 3:
            //No address (fill)
            EmuException::die("[NDMA] Source update method 3 (fill) selected!");
    }

    int dest_multiplier = 0;
    switch (ndma_chan[chan].dest_update_method)
    {
        case 0:
            //Increment
            dest_multiplier = 4;
            break;
        case 1:
            //Decrement
            dest_multiplier = -4;
            break;
        case 2:
            //Fixed
            dest_multiplier = 0;
            break;
        default:
            EmuException::die("[NDMA] Invalid dest update method %d", ndma_chan[chan].dest_update_method);
    }

    int block_size = ndma_chan[chan].write_count;

    //Write a logical block's worth of words
    for (int i = 0; i < block_size; i++)
    {
        uint32_t word = e->arm9_read32(ndma_chan[chan].int_src + (i * src_multiplier));
        e->arm9_write32(ndma_chan[chan].int_dest + (i * dest_multiplier), word);
    }

    //If reload flags are set, dest/source remain the same
    if (!ndma_chan[chan].dest_reload)
        ndma_chan[chan].int_src += (block_size * src_multiplier);

    if (!ndma_chan[chan].src_reload)
        ndma_chan[chan].int_dest += (block_size * dest_multiplier);

    if (ndma_chan[chan].imm_mode)
    {
        ndma_chan[chan].busy = false;
        printf("[NDMA] Chan%d finished!\n", chan);

        if (ndma_chan[chan].irq_enable)
            int9->assert_irq(chan);
    }
    else if (!ndma_chan[chan].repeating_mode)
    {
        ndma_chan[chan].transfer_count -= block_size;
        if (!ndma_chan[chan].transfer_count)
        {
            ndma_chan[chan].busy = false;
            printf("[NDMA] Chan%d finished!\n", chan);

            if (ndma_chan[chan].irq_enable)
                int9->assert_irq(chan);
        }
    }
}

uint32_t DMA9::read32_ndma(uint32_t addr)
{
    addr &= 0xFF;
    if (addr == 0)
        return global_ndma_ctrl;
    if (addr >= 0x4)
    {
        int index = (addr - 4) / 0x1C;
        int reg = (addr - 4) % 0x1C;

        if (index > 7)
            return 0;

        switch (reg)
        {
            case 0x00:
                return ndma_chan[index].source_addr;
            case 0x04:
                return ndma_chan[index].dest_addr;
            case 0x08:
                return ndma_chan[index].transfer_count;
            case 0x0C:
                return ndma_chan[index].write_count;
            case 0x14:
                return ndma_chan[index].fill_data;
            case 0x18:
            {
                uint32_t reg = 0;
                reg |= ndma_chan[index].dest_update_method << 10;
                reg |= ndma_chan[index].dest_reload << 12;
                reg |= ndma_chan[index].src_update_method << 13;
                reg |= ndma_chan[index].src_reload << 15;
                reg |= ndma_chan[index].words_per_block << 16;
                reg |= ndma_chan[index].startup_mode << 24;
                reg |= ndma_chan[index].imm_mode << 28;
                reg |= ndma_chan[index].repeating_mode << 29;
                reg |= ndma_chan[index].irq_enable << 30;
                reg |= ndma_chan[index].busy << 31;
                printf("[NDMA] Read chan%d ctrl: $%08X\n", index, reg);
                return reg;
            }
        }
    }
    printf("[NDMA] Unrecognized read32 $%08X\n", addr);
    return 0;
}

uint32_t DMA9::read32_xdma(uint32_t addr)
{
    return xdma.read32(addr);
}

void DMA9::write32_ndma(uint32_t addr, uint32_t value)
{
    addr &= 0xFF;

    if (addr == 0x0)
    {
        printf("[NDMA] Write global control: $%08X\n", value);
        global_ndma_ctrl = value;
        return;
    }

    if (addr >= 0x4)
    {
        int index = (addr - 4) / 0x1C;
        int reg = (addr - 4) % 0x1C;

        if (index > 7)
            return;

        switch (reg)
        {
            case 0x00:
                printf("[NDMA] Write chan%d source addr: $%08X\n", index, value);
                ndma_chan[index].source_addr = value & 0xFFFFFFFC;
                break;
            case 0x04:
                printf("[NDMA] Write chan%d dest addr: $%08X\n", index, value);
                ndma_chan[index].dest_addr = value & 0xFFFFFFFC;
                break;
            case 0x08:
                printf("[NDMA] Write chan%d transfer count: $%08X\n", index, value);
                ndma_chan[index].transfer_count = value & 0x0FFFFFFF;
                break;
            case 0x0C:
                printf("[NDMA] Write chan%d write count: $%08X\n", index, value);
                ndma_chan[index].write_count = value & 0x00FFFFFF;
                break;
            case 0x10:
                printf("[NDMA] Write chan%d block interval: $%08X\n", index, value);
                break;
            case 0x14:
                printf("[NDMA] Write chan%d fill: $%08X\n", index, value);
                ndma_chan[index].fill_data = value;
                break;
            case 0x18:
            {
                printf("[NDMA] Write chan%d control: $%08X\n", index, value);

                ndma_chan[index].dest_update_method = (value >> 10) & 0x3;
                ndma_chan[index].dest_reload = value & (1 << 12);
                ndma_chan[index].src_update_method = (value >> 13) & 0x3;
                ndma_chan[index].src_reload = value & (1 << 15);
                ndma_chan[index].words_per_block = (value >> 16) & 0xF;
                ndma_chan[index].startup_mode = (NDMA_Request)((value >> 24) & 0xF);
                ndma_chan[index].imm_mode = value & (1 << 28);
                ndma_chan[index].repeating_mode = value & (1 << 29);
                ndma_chan[index].irq_enable = value & (1 << 30);

                bool old_busy = ndma_chan[index].busy;
                ndma_chan[index].busy = value & (1 << 31);

                //Start NDMA transfer
                if (!old_busy && ndma_chan[index].busy)
                {
                    ndma_chan[index].int_src = ndma_chan[index].source_addr;
                    ndma_chan[index].int_dest = ndma_chan[index].dest_addr;
                    if (ndma_chan[index].imm_mode)
                        run_ndma(index);
                }
                break;
            }
        }

        return;
    }
    printf("[NDMA] Unrecognized write32 $%08X: $%08X\n", addr, value);
}

void DMA9::write32_xdma(uint32_t addr, uint32_t value)
{
    xdma.write32(addr, value);
}
