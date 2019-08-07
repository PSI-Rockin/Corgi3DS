#include "common/common.hpp"
#include "corelink_dma.hpp"

//#define printf(fmt, ...)(0)

Corelink_DMA::Corelink_DMA()
{
    mem_read8 = nullptr;
    mem_read32 = nullptr;
    mem_write32 = nullptr;
    send_interrupt = nullptr;
}

void Corelink_DMA::reset()
{
    memset(pending_reqs, 0, sizeof(pending_reqs));
    for (int i = 0; i < 8; i++)
    {
        dma[i].state = Corelink_Chan::Status::STOP;

        //Empty the FIFO
        std::queue<uint32_t> empty;
        dma[i].fifo.swap(empty);
    }

    command_set = false;
    command = 0;
    param_count = 0;
    params_needed = 0;

    int_enable = 0;
    int_flag = 0;
    sev_flag = 0;
}

void Corelink_DMA::run()
{
    //TODO: can the DMA manager thread run on its own? This code assumes it can't
    for (int i = 0; i < 8; i++)
    {
        if (dma[i].state == Corelink_Chan::Status::WFP)
        {
            if (pending_reqs[dma[i].peripheral])
                dma[i].state = Corelink_Chan::Status::EXEC;
        }
        //TODO: Check if programs care about transfers being instant
        while (dma[i].state == Corelink_Chan::Status::EXEC)
        {
            uint8_t byte = mem_read8(dma[i].PC);
            dma[i].PC++;

            exec_instr(byte, i);
        }
    }
}

void Corelink_DMA::set_pending(int index)
{
    pending_reqs[index] = true;
}

void Corelink_DMA::clear_pending(int index)
{
    pending_reqs[index] = false;
}

void Corelink_DMA::set_mem_read8_func(std::function<uint8_t (uint32_t)> func)
{
    mem_read8 = func;
}

void Corelink_DMA::set_mem_read32_func(std::function<uint32_t (uint32_t)> func)
{
    mem_read32 = func;
}

void Corelink_DMA::set_mem_write32_func(std::function<void (uint32_t, uint32_t)> func)
{
    mem_write32 = func;
}

void Corelink_DMA::set_send_interrupt(std::function<void (int)> func)
{
    send_interrupt = func;
}

uint32_t Corelink_DMA::read32(uint32_t addr)
{
    addr &= 0xFFF;

    if (addr >= 0x100 && addr < 0x140)
    {
        int index = (addr / 8) & 0x7;
        if (addr & 0x4)
            return dma[index].PC;

        return dma[index].state;
    }

    if (addr >= 0x400 && addr < 0x500)
    {
        int index = (addr / 0x20) & 0x7;

        switch (addr & 0x1F)
        {
            case 0x00:
                return dma[index].source_addr;
            case 0x04:
                return dma[index].dest_addr;
            case 0x0C:
                return dma[index].loop_ctr[0];
            case 0x10:
                return dma[index].loop_ctr[1];
        }
    }

    switch (addr)
    {
        case 0x020:
            printf("[Corelink] Read IE: $%08X\n", int_enable);
            return int_enable;
        case 0x024:
            printf("[Corelink] Read SEV IF: $%08X\n", sev_flag);
            return sev_flag;
        case 0x028:
            printf("[Corelink] Read IF: $%08X\n", int_flag);
            return int_flag;
    }
    printf("[Corelink] Unrecognized read32 $%08X\n", addr);
    return 0;
}

void Corelink_DMA::write32(uint32_t addr, uint32_t value)
{
    addr &= 0xFFF;
    switch (addr)
    {
        case 0x020:
            printf("[Corelink] Write IE: $%08X\n", value);
            int_enable = value;
            return;
        case 0x02C:
            printf("[Corelink] Write IF: $%08X\n", value);
            sev_flag &= ~value;
            int_flag &= ~value;
            return;
        case 0xD04:
            if ((value & 0x3) == 0)
                exec_debug();
            else
                EmuException::die("[Corelink] Reserved value $%02X passed to exec debug\n", value & 0x3);
            return;
        case 0xD08:
            debug_instrs[0] = value;
            return;
        case 0xD0C:
            debug_instrs[1] = value;
            return;
    }
    printf("[Corelink] Unrecognized write32 $%08X: $%08X\n", addr, value);
}

void Corelink_DMA::exec_debug()
{
    printf("Instr: $%08X $%08X\n", debug_instrs[0], debug_instrs[1]);
    int chan = 0;
    if (debug_instrs[0] & 0x1)
        chan = (debug_instrs[0] >> 8) & 0x7;
    else
        chan = 8; //DMA manager thread

    uint8_t instr_buffer[6];
    instr_buffer[0] = (debug_instrs[0] >> 16) & 0xFF;
    instr_buffer[1] = debug_instrs[0] >> 24;
    *(uint32_t*)&instr_buffer[2] = debug_instrs[1];

    dma[chan].state = Corelink_Chan::Status::EXEC;

    for (int i = 0; i < 6; i++)
    {
        exec_instr(instr_buffer[i], chan);
        if (dma[chan].state == Corelink_Chan::Status::STOP)
            break;
    }

    dma[chan].state = Corelink_Chan::Status::STOP;
}

void Corelink_DMA::exec_instr(uint8_t byte, int chan)
{
    //Fetch and decode a new instruction
    if (!command_set)
    {
        command = byte;
        switch (command)
        {
            case 0x00:
                printf("[Corelink] DMAEND\n");
                dma[chan].state = Corelink_Chan::Status::STOP;
                break;
            case 0x01:
                printf("[Corelink] DMAKILL\n");
                dma[chan].state = Corelink_Chan::Status::STOP;
                break;
            case 0x04:
                printf("[Corelink] DMALD: chan%d\n", chan);
                instr_ld(chan, command & 0x3);
                break;
            case 0x08:
            case 0x0B:
                printf("[Corelink] DMAST: chan%d\n", chan);
                instr_st(chan, command & 0x3);
                break;
            case 0x12:
                //Don't need to do anything here, since all memory accesses are done instantly
                printf("[Corelink] DMARMB\n");
                break;
            case 0x13:
                //Same as DMARMB
                printf("[Corelink] DMAWMB\n");
                break;
            case 0x18:
                printf("[Corelink] DMANOP\n");
                break;
            case 0x20:
            case 0x22:
                //DMALP
                params_needed = 1;
                command_set = true;
                break;
            case 0x25:
            case 0x27:
                //DMALDP
                params_needed = 1;
                command_set = true;
                break;
            case 0x29:
            case 0x2B:
                params_needed = 1;
                command_set = true;
                break;
            case 0x30:
            case 0x31:
            case 0x32:
                //DMAWFP
                params_needed = 1;
                command_set = true;
                break;
            case 0x34:
                //DMASEV
                params_needed = 1;
                command_set = true;
                break;
            case 0x35:
                //DMAFLUSHP
                params_needed = 1;
                command_set = true;
                break;
            case 0x38:
            case 0x3C:
                //DMALPEND
                params_needed = 1;
                command_set = true;
                break;
            case 0xA2:
                //DMAGO
                params_needed = 5;
                command_set = true;
                break;
            case 0xBC:
                //DMAMOV
                params_needed = 5;
                command_set = true;
                break;
            default:
                EmuException::die("[Corelink] Unrecognized opcode $%02X\n", byte);
        }
    }
    else
    {
        params[param_count] = byte;
        param_count++;

        if (param_count >= params_needed)
        {
            switch (command)
            {
                case 0x20:
                case 0x22:
                    instr_lp(chan, (command >> 1) & 0x1);
                    break;
                case 0x25:
                case 0x27:
                    instr_ldp(chan, (command >> 1) & 0x1);
                    break;
                case 0x29:
                case 0x2B:
                    instr_stp(chan, (command >> 1) & 0x1);
                    break;
                case 0x30:
                case 0x31:
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
            command_set = false;
            param_count = 0;
        }
    }
}

void Corelink_DMA::instr_ld(int chan, uint8_t modifier)
{
    if (modifier == 1)
        return;

    if (dma[chan].ctrl.endian_swap_size)
        EmuException::die("[Corelink] Endian size for LD");

    int load_size = dma[chan].ctrl.src_burst_size;
    load_size *= dma[chan].ctrl.src_burst_len;

    if (load_size & 0x3)
        EmuException::die("[Corelink] Load size not word aligned for LD");

    int multiplier = (int)dma[chan].ctrl.inc_src;
    uint32_t addr = dma[chan].source_addr;
    for (int i = 0; i < load_size; i += 4)
    {
        uint32_t word = mem_read32(addr + (multiplier * i));
        dma[chan].fifo.push(word);
    }

    dma[chan].source_addr += (load_size * multiplier);
}

void Corelink_DMA::instr_st(int chan, uint8_t modifier)
{
    if (modifier == 1)
        return;

    if (dma[chan].ctrl.endian_swap_size)
        EmuException::die("[Corelink] Endian size for ST");

    int store_size = dma[chan].ctrl.dest_burst_size;
    store_size *= dma[chan].ctrl.dest_burst_len;

    if (store_size & 0x3)
        EmuException::die("[Corelink] Store size not word aligned for ST");

    int multiplier = (int)dma[chan].ctrl.inc_dest;
    uint32_t addr = dma[chan].dest_addr;
    for (int i = 0; i < store_size; i += 4)
    {
        uint32_t word = dma[chan].fifo.front();
        mem_write32(addr + (multiplier * i), word);
        dma[chan].fifo.pop();
    }

    dma[chan].dest_addr += (store_size * multiplier);
}

void Corelink_DMA::instr_lp(int chan, int loop_ctr_index)
{
    uint16_t iterations = params[0];
    dma[chan].loop_ctr[loop_ctr_index] = iterations;
    printf("[Corelink] DMALP%d: chan%d iterations: %d\n", loop_ctr_index, chan, iterations);
}

void Corelink_DMA::instr_ldp(int chan, bool burst)
{
    if (!burst)
        return;

    if (dma[chan].ctrl.endian_swap_size)
        EmuException::die("[Corelink] Endian size for LDP");

    int load_size = dma[chan].ctrl.src_burst_size;
    load_size *= dma[chan].ctrl.src_burst_len;

    if (load_size & 0x3)
        EmuException::die("[Corelink] Load size not word aligned for LDP");

    //There is a peripheral byte here, but we ignore it
    printf("[Corelink] LDP: chan%d\n", chan);

    int multiplier = (int)dma[chan].ctrl.inc_src;
    uint32_t addr = dma[chan].source_addr;
    for (int i = 0; i < load_size; i += 4)
    {
        uint32_t word = mem_read32(addr + (i * multiplier));
        //printf("[Corelink] Load $%08X: $%08X\n", addr + (i * multiplier), word);
        dma[chan].fifo.push(word);
    }
    dma[chan].source_addr += (load_size * multiplier);
}

void Corelink_DMA::instr_stp(int chan, bool burst)
{
    if (!burst)
        return;

    if (dma[chan].ctrl.endian_swap_size)
        EmuException::die("[Corelink] Endian size for ST");

    int store_size = dma[chan].ctrl.dest_burst_size;
    store_size *= dma[chan].ctrl.dest_burst_len;

    if (store_size & 0x3)
        EmuException::die("[Corelink] Store size not word aligned for ST");

    int multiplier = (int)dma[chan].ctrl.inc_dest;
    uint32_t addr = dma[chan].dest_addr;
    for (int i = 0; i < store_size; i += 4)
    {
        uint32_t word = dma[chan].fifo.front();
        mem_write32(addr + (multiplier * i), word);
        dma[chan].fifo.pop();
    }

    dma[chan].dest_addr += (store_size * multiplier);
}

void Corelink_DMA::instr_wfp(int chan)
{
    int peripheral = params[0] >> 3;
    dma[chan].state = Corelink_Chan::Status::WFP;
    dma[chan].peripheral = peripheral;

    printf("[Corelink] DMAWFP: chan%d, peripheral %d\n", chan, peripheral);
}

void Corelink_DMA::instr_sev(int chan)
{
    int event = params[0] >> 3;

    printf("[Corelink] DMASEV: chan%d event%d\n", chan, event);

    event = 1 << event;

    if (int_enable & event)
    {
        int_flag |= event;
        sev_flag |= event;
        send_interrupt(params[0] >> 3);
    }
    //else
        //EmuException::die("[Corelink] No IRQ registered for DMASEV event");
}

void Corelink_DMA::instr_flushp()
{
    int peripheral = params[0] >> 3;

    //TODO: We need to check with the peripheral so it can resend a DMA request
    pending_reqs[peripheral] = false;

    printf("[Corelink] DMAFLUSHP: peripheral: %d\n", peripheral);
}

void Corelink_DMA::instr_lpend(int chan)
{
    int index = (command >> 2) & 0x1;
    bool loop_finite = (command >> 4) & 0x1;

    if (!loop_finite)
    {
        printf("[Corelink] DMALPFE!\n");
        return;
    }

    //Add +2 to account for two bytes of the instruction
    uint16_t jump = params[0] + 2;
    printf("[Corelink] DMALPEND: chan%d, jump offset: $%02X\n", chan, jump);

    if (dma[chan].loop_ctr[index])
    {
        dma[chan].loop_ctr[index]--;
        printf("New loop: %d\n", dma[chan].loop_ctr[index]);
        dma[chan].PC -= jump;
    }
}

void Corelink_DMA::instr_dmago()
{
    int chan = params[0] & 0x7;
    uint32_t start = 0;
    for (int i = 0; i < 4; i++)
        start |= (params[i + 1] & 0xFF) << (i * 8);

    dma[chan].state = Corelink_Chan::Status::EXEC;
    dma[chan].PC = start;

    printf("[Corelink] DMAGO: chan%d, PC: $%08X\n", chan, start);
}

void Corelink_DMA::instr_mov(int chan)
{
    int reg = params[0] & 0x7;
    uint32_t value = 0;
    for (int i = 0; i < 4; i++)
        value |= (params[i + 1] & 0xFF) << (i * 8);

    printf("[Corelink] DMAMOV reg%d: $%08X\n", reg, value);

    switch (reg)
    {
        case 0x0:
            dma[chan].source_addr = value;
            break;
        case 0x1:
            write_chan_ctrl(chan, value);
            break;
        case 0x2:
            dma[chan].dest_addr = value;
            break;
        default:
            EmuException::die("[Corelink] Unrecognized DMAMOV reg %d\n", reg);
    }
}

void Corelink_DMA::write_chan_ctrl(int chan, uint32_t value)
{
    printf("[Corelink] Write chan%d ctrl: $%08X\n", chan, value);

    dma[chan].ctrl.inc_src = value & 0x1;
    dma[chan].ctrl.src_burst_size = 1 << ((value >> 1) & 0x7);
    dma[chan].ctrl.src_burst_len = ((value >> 4) & 0xF) + 1;
    dma[chan].ctrl.inc_dest = value & (1 << 14);
    dma[chan].ctrl.dest_burst_size = 1 << ((value >> 15) & 0x7);
    dma[chan].ctrl.dest_burst_len = ((value >> 18) & 0xF) + 1;
    dma[chan].ctrl.endian_swap_size = (value >> 28) & 0x7;

    printf("Inc src: %d Inc dest: %d\n", dma[chan].ctrl.inc_src, dma[chan].ctrl.inc_dest);
    printf("Src burst size: %d\n", dma[chan].ctrl.src_burst_size);
    printf("Src burst len: %d\n", dma[chan].ctrl.src_burst_len);
    printf("Dest burst size: %d\n", dma[chan].ctrl.dest_burst_size);
    printf("Dest burst len: %d\n", dma[chan].ctrl.dest_burst_len);
    printf("Endian swap size: %d\n", dma[chan].ctrl.endian_swap_size);
}
