#include <cstdio>
#include <cstring>
#include "dma9.hpp"
#include "../common/common.hpp"
#include "../emulator.hpp"

DMA9::DMA9(Emulator* e, Interrupt9* int9) : e(e), int9(int9)
{

}

void DMA9::reset()
{
    memset(pending_ndma_reqs, 0, sizeof(pending_ndma_reqs));
    memset(pending_xdma_reqs, 0, sizeof(pending_xdma_reqs));
    memset(ndma_chan, 0, sizeof(ndma_chan));

    for (int i = 0; i < 8; i++)
    {
        xdma_chan[i].state = XDMA_Chan::Status::STOP;

        //Empty the FIFO
        std::queue<uint32_t> empty;
        xdma_chan[i].fifo.swap(empty);
    }

    xdma_command_set = false;
    xdma_command = 0;
    xdma_param_count = 0;
    xdma_params_needed = 0;

    global_ndma_ctrl = 0;
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
    //TODO: can the DMA manager thread run on its own? This code assumes it can't
    for (int i = 0; i < 8; i++)
    {
        if (xdma_chan[i].state == XDMA_Chan::Status::WFP)
        {
            if (pending_xdma_reqs[xdma_chan[i].peripheral])
                xdma_chan[i].state = XDMA_Chan::Status::EXEC;
        }
        //TODO: Check if programs care about transfers being instant
        while (xdma_chan[i].state == XDMA_Chan::Status::EXEC)
        {
            uint8_t byte = e->arm9_read8(xdma_chan[i].PC);
            xdma_chan[i].PC++;

            xdma_exec_instr(byte, i);
        }
    }
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
    pending_xdma_reqs[req] = true;
}

void DMA9::clear_xdma_req(XDMA_Request req)
{
    pending_xdma_reqs[req] = false;
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
    if (addr >= 0x1000C100 && addr < 0x1000C140)
    {
        int index = (addr / 8) & 0x7;
        if (addr & 0x4)
            return xdma_chan[index].PC;

        return xdma_chan[index].state;
    }

    switch (addr)
    {
        case 0x1000C020:
            printf("[XDMA] Read IE: $%08X\n", xdma_ie);
            return xdma_ie;
        case 0x1000C028:
            printf("[XDMA] Read IF: $%08X\n", xdma_if);
            return xdma_if;
    }
    printf("[XDMA] Unrecognized read32 $%08X\n", addr);
    return 0;
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
    switch (addr)
    {
        case 0x1000C020:
            printf("[XDMA] Write IE: $%08X\n", value);
            xdma_ie = value;
            return;
        case 0x1000C02C:
            printf("[XDMA] Write IF: $%08X\n", value);
            xdma_if &= ~value;
            return;
        case 0x1000CD04:
            if ((value & 0x3) == 0)
                xdma_exec_debug();
            else
                EmuException::die("[XDMA] Reserved value $%02X passed to exec debug\n", value & 0x3);
            return;
        case 0x1000CD08:
            xdma_debug_instrs[0] = value;
            return;
        case 0x1000CD0C:
            xdma_debug_instrs[1] = value;
            return;
    }
    printf("[XDMA] Unrecognized write32 $%08X: $%08X\n", addr, value);
}

void DMA9::xdma_exec_debug()
{
    printf("Instr: $%08X $%08X\n", xdma_debug_instrs[0], xdma_debug_instrs[1]);
    int chan = 0;
    if (xdma_debug_instrs[0] & 0x1)
        chan = (xdma_debug_instrs[0] >> 8) & 0x7;
    else
        chan = 8; //DMA manager thread

    uint8_t instr_buffer[6];
    instr_buffer[0] = (xdma_debug_instrs[0] >> 16) & 0xFF;
    instr_buffer[1] = xdma_debug_instrs[0] >> 24;
    *(uint32_t*)&instr_buffer[2] = xdma_debug_instrs[1];

    xdma_chan[chan].state = XDMA_Chan::Status::EXEC;

    for (int i = 0; i < 6; i++)
    {
        xdma_exec_instr(instr_buffer[i], chan);
        if (xdma_chan[chan].state == XDMA_Chan::Status::STOP)
            break;
    }

    xdma_chan[chan].state = XDMA_Chan::Status::STOP;
}

void DMA9::xdma_exec_instr(uint8_t byte, int chan)
{
    //Fetch and decode a new instruction
    if (!xdma_command_set)
    {
        xdma_command = byte;
        switch (xdma_command)
        {
            case 0x00:
                printf("[XDMA] DMAEND\n");
                xdma_chan[chan].state = XDMA_Chan::Status::STOP;
                break;
            case 0x01:
                printf("[XDMA] DMAKILL\n");
                xdma_chan[chan].state = XDMA_Chan::Status::STOP;
                break;
            case 0x0B:
                printf("[XDMA] DMAST: chan%d\n", chan);
                instr_st(chan, xdma_command & 0x3);
                break;
            case 0x12:
                //Don't need to do anything here, since all memory accesses are done instantly
                printf("[XDMA] DMARMB\n");
                break;
            case 0x13:
                //Same as DMARMB
                printf("[XDMA] DMAWMB\n");
                break;
            case 0x18:
                printf("[XDMA] DMANOP\n");
                break;
            case 0x20:
            case 0x22:
                //DMALP
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0x27:
                //DMALDP
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0x32:
                //DMAWFP
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0x34:
                //DMASEV
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0x35:
                //DMAFLUSHP
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0x38:
            case 0x3C:
                //DMALPEND
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0xA2:
                //DMAGO
                xdma_params_needed = 5;
                xdma_command_set = true;
                break;
            case 0xBC:
                //DMAMOV
                xdma_params_needed = 5;
                xdma_command_set = true;
                break;
            default:
                EmuException::die("[XDMA] Unrecognized opcode $%02X\n", byte);
        }
    }
    else
    {
        xdma_params[xdma_param_count] = byte;
        xdma_param_count++;

        if (xdma_param_count >= xdma_params_needed)
        {
            switch (xdma_command)
            {
                case 0x20:
                case 0x22:
                    instr_lp(chan, (xdma_command >> 1) & 0x1);
                    break;
                case 0x27:
                    instr_ldp(chan, (xdma_command >> 1) & 0x1);
                    break;
                case 0x32:
                    instr_wfp(chan);
                    break;
                case 0x34:
                    instr_sev(chan);
                    break;
                case 0x35:
                    instr_flushp();
                    break;
                case 0x38:
                case 0x3C:
                    instr_lpend(chan);
                    break;
                case 0xA2:
                    instr_dmago();
                    break;
                case 0xBC:
                    instr_mov(chan);
                    break;
            }
            xdma_command_set = false;
            xdma_param_count = 0;
        }
    }
}

void DMA9::instr_st(int chan, uint8_t modifier)
{
    if ((modifier & 0x2) == 0)
        EmuException::die("[XDMA] Single mode for ST");

    if (xdma_chan[chan].ctrl.endian_swap_size)
        EmuException::die("[XDMA] Endian size for ST");

    int store_size = xdma_chan[chan].ctrl.dest_burst_size;
    if (store_size & 0x3)
        EmuException::die("[XDMA] Store size not word aligned for ST");

    int multiplier = (int)xdma_chan[chan].ctrl.inc_dest;
    uint32_t addr = xdma_chan[chan].dest_addr;
    for (int i = 0; i < store_size; i += 4)
    {
        uint32_t word = xdma_chan[chan].fifo.front();
        //printf("[XDMA] Write32 $%08X: $%08X\n", addr + (multiplier * i), word);
        e->arm9_write32(addr + (multiplier * i), word);
        xdma_chan[chan].fifo.pop();
    }

    xdma_chan[chan].dest_addr += (store_size * multiplier);
}

void DMA9::instr_lp(int chan, int loop_ctr_index)
{
    uint16_t iterations = xdma_params[0];
    xdma_chan[chan].loop_ctr[loop_ctr_index] = iterations;
    printf("[XDMA] DMALP%d: chan%d iterations: %d\n", loop_ctr_index, chan, iterations);
}

void DMA9::instr_ldp(int chan, bool burst)
{
    if (!burst)
        EmuException::die("[XDMA] Single mode for LDP");

    if (xdma_chan[chan].ctrl.endian_swap_size)
        EmuException::die("[XDMA] Endian size for LDP");

    int load_size = xdma_chan[chan].ctrl.src_burst_size;
    if (load_size & 0x3)
        EmuException::die("[XDMA] Load size not word aligned for LDP");

    //There is a peripheral byte here, but we ignore it
    printf("[XDMA] LDP: chan%d\n", chan);

    int multiplier = (int)xdma_chan[chan].ctrl.inc_src;
    uint32_t addr = xdma_chan[chan].source_addr;
    for (int i = 0; i < load_size; i += 4)
    {
        uint32_t word = e->arm9_read32(addr + (i * multiplier));
        xdma_chan[chan].fifo.push(word);
    }
    xdma_chan[chan].source_addr += (load_size * multiplier);
}

void DMA9::instr_wfp(int chan)
{
    int peripheral = xdma_params[0] >> 3;
    xdma_chan[chan].state = XDMA_Chan::Status::WFP;
    xdma_chan[chan].peripheral = peripheral;

    printf("[XDMA] DMAWFP: chan%d, peripheral %d\n", chan, peripheral);
}

void DMA9::instr_sev(int chan)
{
    int event = xdma_params[0] >> 3;

    printf("[XDMA] DMASEV: chan%d event%d\n", chan, event);

    event = 1 << event;

    if (xdma_ie & event)
    {
        xdma_if |= event;
        int9->assert_irq(28);
    }
    //else
        //EmuException::die("[XDMA] No IRQ registered for DMASEV event");
}

void DMA9::instr_flushp()
{
    int peripheral = xdma_params[0] >> 3;

    //TODO: We need to check with the peripheral so it can resend a DMA request
    pending_xdma_reqs[peripheral] = false;

    printf("[XDMA] DMAFLUSHP: peripheral: %d\n", peripheral);
}

void DMA9::instr_lpend(int chan)
{
    int index = (xdma_command >> 2) & 0x1;
    bool loop_finite = (xdma_command >> 4) & 0x1;

    if (!loop_finite)
        EmuException::die("[XDMA] Loop forever on DMALPEND");

    //Add +2 to account for two bytes of the instruction
    uint16_t jump = xdma_params[0] + 2;
    printf("[XDMA] DMALPEND: chan%d, jump offset: $%02X\n", chan, jump);

    if (xdma_chan[chan].loop_ctr[index])
    {
        xdma_chan[chan].loop_ctr[index]--;
        printf("New loop: %d\n", xdma_chan[chan].loop_ctr[index]);
        xdma_chan[chan].PC -= jump;
    }
}

void DMA9::instr_dmago()
{
    int chan = xdma_params[0] & 0x7;
    uint32_t start = 0;
    for (int i = 0; i < 4; i++)
        start |= (xdma_params[i + 1] & 0xFF) << (i * 8);

    xdma_chan[chan].state = XDMA_Chan::Status::EXEC;
    xdma_chan[chan].PC = start;

    printf("[XDMA] DMAGO: chan%d, PC: $%08X\n", chan, start);
}

void DMA9::instr_mov(int chan)
{
    int reg = xdma_params[0] & 0x7;
    uint32_t value = 0;
    for (int i = 0; i < 4; i++)
        value |= (xdma_params[i + 1] & 0xFF) << (i * 8);

    printf("[XDMA] DMAMOV reg%d: $%08X\n", reg, value);

    switch (reg)
    {
        case 0x0:
            xdma_chan[chan].source_addr = value;
            break;
        case 0x1:
            xdma_write_chan_ctrl(chan, value);
            break;
        case 0x2:
            xdma_chan[chan].dest_addr = value;
            break;
        default:
            EmuException::die("[XDMA] Unrecognized DMAMOV reg %d\n", reg);
    }
}

void DMA9::xdma_write_chan_ctrl(int chan, uint32_t value)
{
    printf("[XDMA] Write chan%d ctrl: $%08X\n", chan, value);

    xdma_chan[chan].ctrl.inc_src = value & 0x1;
    xdma_chan[chan].ctrl.src_burst_size = ((value >> 4) & 0xF) + 1;
    xdma_chan[chan].ctrl.src_burst_size <<= (value >> 1) & 0x7;
    xdma_chan[chan].ctrl.inc_dest = value & (1 << 14);
    xdma_chan[chan].ctrl.dest_burst_size = ((value >> 18) & 0xF) + 1;
    xdma_chan[chan].ctrl.dest_burst_size <<= (value >> 15) & 0x7;
    xdma_chan[chan].ctrl.endian_swap_size = (value >> 28) & 0x7;

    printf("Inc src: %d Inc dest: %d\n", xdma_chan[chan].ctrl.inc_src, xdma_chan[chan].ctrl.inc_dest);
    printf("Src burst size: %d\n", xdma_chan[chan].ctrl.src_burst_size);
    printf("Dest burst size: %d\n", xdma_chan[chan].ctrl.dest_burst_size);
    printf("Endian swap size: %d\n", xdma_chan[chan].ctrl.endian_swap_size);
}
