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

uint32_t DMA9::read32_ndma(uint32_t addr)
{
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
                case 0x18:
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

void DMA9::instr_wfp(int chan)
{
    int peripheral = xdma_params[0] >> 3;
    xdma_chan[chan].state = XDMA_Chan::Status::WFP;

    printf("[XDMA] DMAWFP: chan%d, peripheral %d\n", chan, peripheral);
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

void DMA9::instr_flushp()
{
    int peripheral = xdma_params[0] >> 3;
    printf("[XDMA] FLUSHP: peripheral: %d\n", peripheral);
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
