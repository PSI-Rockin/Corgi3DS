#include <cstdio>
#include <cstring>
#include "dma9.hpp"
#include "../common/common.hpp"
#include "../emulator.hpp"

DMA9::DMA9(Emulator* e) : e(e)
{

}

void DMA9::reset()
{
    memset(ndma_chan, 0, sizeof(ndma_chan));
    memset(xdma_chan, 0, sizeof(xdma_chan));

    xdma_command_set = false;
    xdma_command = 0;
    xdma_param_count = 0;
    xdma_params_needed = 0;
}

void DMA9::run_xdma()
{
    //TODO: can the DMA manager thread run on its own? This code assumes it can't
    for (int i = 0; i < 7; i++)
    {
        //TODO: Check if programs care about transfers being instant
        while (xdma_chan[i].state == XDMA_Chan::Status::EXEC)
        {
            uint8_t byte = e->arm9_read8(xdma_chan[i].PC);
            xdma_chan[i].PC++;

            xdma_exec_instr(byte, i);
        }
    }
}

void DMA9::ndma_req(NDMA_Request req)
{
    bool dma_ran = false;
    for (int i = 0; i < 8; i++)
    {
        if (ndma_chan[i].startup_mode == req && ndma_chan[i].busy)
        {
            run_ndma(i);
            dma_ran = true;
        }
    }
}

void DMA9::run_ndma(int chan)
{
    //EmuException::die("Run NDMA%d", chan);

    NDMA_Request req = ndma_chan[chan].startup_mode;
}

uint32_t DMA9::read32_ndma(uint32_t addr)
{
    addr &= 0xFF;
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
            return xdma_ie;
        case 0x1000C028:
            return xdma_if;
    }
    printf("[XDMA] Unrecognized read32 $%08X\n", addr);
    return 0;
}

void DMA9::write32_ndma(uint32_t addr, uint32_t value)
{
    addr &= 0xFF;

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
                //DMAKILL
                printf("[XDMA] DMAKILL\n");
                xdma_chan[chan].state = XDMA_Chan::Status::STOP;
                break;
            case 0x18:
                printf("[XDMA] DMANOP\n");
                break;
            case 0x20:
            case 0x22:
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0x32:
                //DMAWFP
                xdma_params_needed = 1;
                xdma_command_set = true;
                break;
            case 0x35:
                //DMAFLUSHP
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
                case 0x32:
                    instr_wfp(chan);
                    break;
                case 0x35:
                    instr_flushp();
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

void DMA9::instr_lp(int chan, int loop_ctr_index)
{
    uint16_t iterations = xdma_params[0] + 1;
    xdma_chan[chan].loop_ctr[loop_ctr_index] = iterations;
    printf("[XDMA] DMALP%d: chan%d iterations: %d\n", loop_ctr_index, chan, iterations);
}

void DMA9::instr_wfp(int chan)
{
    int peripheral = xdma_params[0] >> 3;
    xdma_chan[chan].state = XDMA_Chan::Status::WFP;

    printf("[XDMA] DMAWFP: chan%d, peripheral %d\n", chan, peripheral);
}

void DMA9::instr_flushp()
{
    int peripheral = xdma_params[0] >> 3;
    printf("[XDMA] FLUSHP: peripheral: %d\n", peripheral);
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
    xdma_chan[chan].ctrl.inc_dest = value & (1 << 16);
    xdma_chan[chan].ctrl.endian_swap_size = (value >> 28) & 0x7;
}
