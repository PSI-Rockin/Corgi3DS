#include <cstdio>
#include <cstring>
#include "../common/common.hpp"
#include "../scheduler.hpp"
#include "dsp.hpp"
#include "dsp_interpreter.hpp"
#include "signextend.hpp"

DSP::DSP(Scheduler* scheduler) : scheduler(scheduler)
{
    set_cpu_interrupt_sender(nullptr);
}

void DSP::reset(uint8_t* dsp_mem)
{
    this->dsp_mem = dsp_mem;
    reset_core();

    apbp.cpu_sema_recv = 0;
    apbp.cpu_sema_mask = 0;

    reset_signal = false;
}

void DSP::reset_core()
{
    halted = false;
    running = false;
    pc = 0;

    memset(timers, 0, sizeof(timers));

    miu.mmio_base = 0x8000;
    miu.xpage = 0;
    miu.ypage = 0;
    miu.zpage = 0;
    miu.page_mode = false;
    miu.zsp = false;

    memset(apbp.cmd_ready, 0, sizeof(apbp.cmd_ready));
    memset(apbp.reply_ready, 0, sizeof(apbp.reply_ready));
    apbp.dsp_sema_recv = 0;
    apbp.dsp_sema_mask = 0;

    memset(ahbm.burst, 0, sizeof(ahbm.burst));
    memset(ahbm.chan_connection, 0, sizeof(ahbm.chan_connection));
    memset(ahbm.data_type, 0, sizeof(ahbm.data_type));
    memset(ahbm.transfer_dir, 0, sizeof(ahbm.transfer_dir));

    dma.arm_addr = 0;
    dma.fifo_started = false;

    memset(&icu, 0, sizeof(icu));

    btdmp.cycles_per_transmit = 0;
    btdmp.irq_on_empty_transmit = false;
    btdmp.transmit_enabled = false;
    btdmp.transmit_cycles_left = 0;

    std::queue<uint16_t> empty;
    empty.swap(btdmp.transmit_queue);

    memset(&stt2, 0, sizeof(stt2));
    rep_new_pc = 0;
    rep = false;
    repc = 0;
}

void DSP::set_cpu_interrupt_sender(std::function<void()> func)
{
    send_arm_interrupt = func;
}

void DSP::run(int cycles)
{
    if (running)
    {
        while (cycles)
        {
            for (int i = 0; i < 2; i++)
            {
                if (timers[i].enabled)
                {
                    timers[i].counter--;
                    if (timers[i].counter == 0)
                        do_timer_overflow(i);
                }
            }

            //TODO: Move BTDMP processing to the scheduler
            if (btdmp.transmit_enabled)
            {
                btdmp.transmit_cycles_left--;
                if (!btdmp.transmit_cycles_left)
                {
                    btdmp.transmit_cycles_left = btdmp.cycles_per_transmit;
                    if (btdmp.transmit_queue.size())
                    {
                        btdmp.transmit_queue.pop();
                        if (!btdmp.transmit_queue.size() && btdmp.irq_on_empty_transmit)
                            assert_dsp_irq(0xB);
                    }
                }
            }

            if (halted)
            {
                int_check();
                if (halted)
                    return;
            }

            if (rep)
            {
                if (repc == 0)
                    rep = false;
                else
                {
                    rep_new_pc = pc;
                    repc--;
                }
            }

            uint16_t instr = fetch_code_word();

            DSP_Interpreter::interpret(*this, instr);

            //If we've hit the end of a block, jump to the start address after we've executed an instruction.
            if (stt2.lp)
            {
                DSP_BKREP_ELEMENT* cur_level = &bkrep_stack[stt2.bcn - 1];
                if (cur_level->end + 1 == pc)
                {
                    if (cur_level->lc == 0)
                    {
                        stt2.bcn--;
                        stt2.lp = stt2.bcn > 0;
                    }
                    else
                    {
                        pc = cur_level->start;
                        cur_level->lc--;
                    }
                }
            }

            if (rep_new_pc)
            {
                pc = rep_new_pc;
                rep_new_pc = 0;
            }

            int_check();

            //print_state();
            cycles--;
        }
    }
}

void DSP::halt()
{
    halted = true;
}

void DSP::unhalt()
{
    halted = false;
}

void DSP::print_state()
{
    printf("pc:$%05X sp:$%04X\n", pc, sp);
    printf("a0:$%llX a1:$%llX b0:$%llX b1:$%llX\n", a0, a1, b0, b1);
    printf("fz:%d fm:%d fn:%d fv:%d fc:%d fe:%d fvl:%d flm:%d\n",
           stt0.fz, stt0.fm, stt0.fn, stt0.fv, stt0.fc, stt0.fe, stt0.fvl, stt0.flm);
    printf("x0:$%04X x1:$%04X y0:$%04X y1:$%04X\n", x[0], x[1], y[0], y[1]);
    printf("lp:%d bcn:%d\n", stt2.lp, stt2.bcn);
    for (int i = 0; i < 8; i++)
        printf("r%d:$%04X ", i, r[i]);
    printf("\n");
    printf("\n");
}

uint16_t DSP::read16(uint32_t addr)
{
    uint16_t reg = 0;
    switch (addr)
    {
        case 0x10203000:
            switch (dma.mem_type)
            {
                case 0:
                {
                    //Data memory
                    if ((miu.page_mode == 0 && miu.zpage) || (miu.page_mode == 1 && (miu.xpage || miu.ypage)))
                        EmuException::die("[DSP_CPU] Read DMEM FIFO done when x/y/zpage is nonzero");

                    if (dma.arm_addr > 0xFFFF)
                        EmuException::die("[DSP_CPU] Read DMEM FIFO done when ARM addr > 0xFFFF");

                    uint32_t real_addr = convert_addr(dma.arm_addr);
                    reg = *(uint16_t*)&dsp_mem[real_addr];
                }
                    break;
                default:
                    EmuException::die("[DSP_CPU] Unrecognized READ_FIFO memtype %d", dma.mem_type);
            }
            printf("[DSP_CPU] READ FIFO: $%04X ($%08X)\n", reg, dma.arm_addr);
            if (dma.cur_fifo_len != 0xFF)
            {
                dma.cur_fifo_len--;
                if (!dma.cur_fifo_len)
                    dma.fifo_started = false;
            }
            if (dma.auto_inc)
                dma.arm_addr++;
            break;
        case 0x10203008:
            reg |= dma.auto_inc << 1;
            reg |= dma.fifo_len << 2;
            reg |= dma.fifo_started << 4;
            for (int i = 0; i < 3; i++)
                reg |= apbp.reply_int_enable[i] << (i + 9);
            reg |= dma.mem_type << 12;
            break;
        case 0x1020300C:
            reg |= dma.fifo_started << 6; //read FIFO not empty
            reg |= 1 << 8; //write FIFO empty
            reg |= ((apbp.cpu_sema_recv & ~apbp.cpu_sema_mask) != 0) << 9;
            for (int i = 0; i < 3; i++)
            {
                reg |= apbp.reply_ready[i] << (i + 10);
                reg |= apbp.cmd_ready[i] << (i + 13);
            }
            //printf("[DSP_CPU] Read16 PSTS: $%04X\n", reg);
            break;
        case 0x10203010:
            //TODO: Is SET_SEMA readable?
            break;
        case 0x10203014:
            reg = apbp.cpu_sema_mask;
            break;
        case 0x1020301C:
            reg = apbp.cpu_sema_recv;
            break;
        case 0x10203024:
            printf("[DSP_CPU] Read REPLY0: $%04X\n", apbp.reply[0]);
            apbp.reply_ready[0] = false;
            return apbp.reply[0];
        case 0x1020302C:
            printf("[DSP_CPU] Read REPLY1: $%04X\n", apbp.reply[1]);
            apbp.reply_ready[1] = false;
            return apbp.reply[1];
        case 0x10203034:
            printf("[DSP_CPU] Read REPLY2: $%04X\n", apbp.reply[2]);
            apbp.reply_ready[2] = false;
            return apbp.reply[2];
        default:
            EmuException::die("[DSP_CPU] Unrecognized read16 $%08X", addr);
    }
    return reg;
}

void DSP::write16(uint32_t addr, uint16_t value)
{
    switch (addr)
    {
        case 0x10203000:
            switch (dma.mem_type)
            {
                case 0:
                {
                    //Data memory
                    if ((miu.page_mode == 0 && miu.zpage) || (miu.page_mode == 1 && (miu.xpage || miu.ypage)))
                        EmuException::die("[DSP_CPU] Write DMEM FIFO done when x/y/zpage is nonzero");

                    if (dma.arm_addr > 0xFFFF)
                        EmuException::die("[DSP_CPU] Write DMEM FIFO done when ARM addr > 0xFFFF");

                    uint32_t real_addr = convert_addr(dma.arm_addr);
                    *(uint16_t*)&dsp_mem[real_addr] = value;
                }
                    break;
                default:
                    EmuException::die("[DSP_CPU] Unrecognized READ_FIFO memtype %d", dma.mem_type);
            }
            printf("[DSP_CPU] WRITE FIFO: $%04X ($%08X)\n", value, dma.arm_addr);
            if (dma.cur_fifo_len != 0xFF)
            {
                dma.cur_fifo_len--;
                if (!dma.cur_fifo_len)
                    dma.fifo_started = false;
            }
            if (dma.auto_inc)
                dma.arm_addr++;
            break;
        case 0x10203004:
            printf("[DSP_CPU] Write16 PADR: $%04X\n", value);
            dma.arm_addr &= ~0xFFFF;
            dma.arm_addr |= value;
            break;
        case 0x10203008:
            printf("[DSP_CPU] Write16 PCFG: $%04X\n", value);
        {
            bool old_start = dma.fifo_started;
            if (!(value & 0x1) && reset_signal)
                running = true;
            reset_signal = value & 0x1;
            if (reset_signal)
                reset_core();
            dma.auto_inc = (value >> 1) & 0x1;
            dma.fifo_len = (value >> 2) & 0x3;
            dma.fifo_started = (value >> 4) & 0x1;
            apbp.reply_int_enable[0] = (value >> 9) & 0x1;
            apbp.reply_int_enable[1] = (value >> 10) & 0x1;
            apbp.reply_int_enable[2] = (value >> 11) & 0x1;
            dma.mem_type = (value >> 12) & 0xF;

            for (int i = 0; i < 3; i++)
            {
                if (apbp.reply_int_enable[i] && apbp.reply_ready[i])
                    send_arm_interrupt();
            }

            if (!old_start && dma.fifo_started)
            {
                switch (dma.fifo_len)
                {
                    case 0:
                        dma.cur_fifo_len = 1;
                        break;
                    case 1:
                        dma.cur_fifo_len = 8;
                        break;
                    case 2:
                        dma.cur_fifo_len = 16;
                        break;
                    case 3:
                        dma.cur_fifo_len = 0xFF;
                        break;
                }
            }
        }
            break;
        case 0x10203010:
            printf("[DSP_CPU] Write16 sema set: $%04X\n", value);
        {
            uint16_t mask = apbp.dsp_sema_mask;
            uint16_t old_sema = apbp.dsp_sema_recv;
            if (!(old_sema & ~mask) && (value & ~mask))
                assert_dsp_irq(0xE);
            apbp.dsp_sema_recv |= value;
        }
            break;
        case 0x10203014:
            printf("[DSP_CPU] Write16 sema mask: $%04X\n", value);
            apbp.cpu_sema_mask = value;
            break;
        case 0x10203018:
            printf("[DSP_CPU] Write16 sema clear: $%04X\n", value);
            apbp.cpu_sema_recv &= ~value;
            break;
        case 0x10203030:
            printf("[DSP_CPU] Write16 CMD2: $%04X\n", value);
            apbp_send_cmd(2, value);
            break;
        default:
            EmuException::die("[DSP_CPU] Unrecognized write16 $%08X: $%04X", addr, value);
    }
}

uint32_t DSP::convert_addr(uint16_t addr)
{
    uint32_t real_addr = 0x20000; //start of shared data memory
    const static uint32_t bank_size = 0x10000;

    real_addr += addr;

    if (!miu.page_mode)
        real_addr += miu.zpage * bank_size;
    else
    {
        if (addr <= miu.x_size[0] * 0x400)
            real_addr += miu.xpage * bank_size;
        else
            real_addr += miu.ypage * bank_size;
    }

    return real_addr << 1;
}

unsigned int DSP::std20_log2p1(unsigned int value)
{
    if (!value)
        return 0;

    int bits = 31;
    while (!(value >> bits))
        bits--;

    return bits + 1;
}

uint16_t DSP::fetch_code_word()
{
    uint16_t word = *(uint16_t*)&dsp_mem[pc << 1];
    pc++;
    return word;
}

uint16_t DSP::read_program_word(uint32_t addr)
{
    return *(uint16_t*)&dsp_mem[addr << 1];
}

uint16_t DSP::read_data_word(uint16_t addr)
{
    //printf("[DSP] Read data word $%04X\n", addr);
    if (addr >= miu.mmio_base && addr < miu.mmio_base + 0x800)
    {
        //Read from MMIO
        addr &= 0x7FF;
        if (addr >= 0x0E2 && addr < 0x0E2 + (4 * 6))
        {
            //AHBM config registers
            int index = (addr - 0x0E2) / 6;
            int reg = (addr - 0x0E2) % 6;

            uint16_t value = 0;

            switch (reg)
            {
                case 0:
                    value |= ahbm.burst[index] << 1;
                    value |= ahbm.data_type[index] << 4;
                    return value;
                case 2:
                    return ahbm.transfer_dir[index] << 8;
                case 4:
                    return ahbm.chan_connection[index];
                default:
                    EmuException::die("[DSP_AHBM] Unrecognized read16 reg %d\n", reg);
            }
        }
        switch (addr)
        {
            case 0x01A:
                return 0xC902; //chip ID
            case 0x028:
                return timers[0].counter & 0xFFFF;
            case 0x02A:
                return timers[0].counter >> 16;
            case 0x038:
                return timers[1].counter & 0xFFFF;
            case 0x03A:
                return timers[1].counter >> 16;
            case 0x0CA:
                apbp.cmd_ready[2] = false;
                return apbp.cmd[2];
            case 0x0D2:
                return apbp.dsp_sema_recv;
            case 0x0D6:
            {
                uint16_t reg = 0;
                reg |= apbp.reply_ready[0] << 5;
                reg |= apbp.reply_ready[1] << 6;
                reg |= apbp.reply_ready[2] << 7;
                reg |= apbp.cmd_ready[0] << 8;
                reg |= ((apbp.dsp_sema_recv & ~apbp.dsp_sema_mask) != 0) << 9;
                reg |= apbp.cmd_ready[1] << 12;
                reg |= apbp.cmd_ready[2] << 13;
                printf("[DSP] Read STS: $%04X\n", reg);
                return reg;
            }
            case 0x0E0:
                return 0;
            case 0x10E:
                return miu.xpage;
            case 0x110:
                return miu.ypage;
            case 0x112:
                return miu.zpage;
            case 0x114:
            {
                uint16_t reg = 0;
                reg |= miu.x_size[0];
                reg |= miu.y_size[0] << 8;
                return reg;
            }
            case 0x11A:
            {
                uint16_t reg = 0;
                reg |= miu.zsp << 4;
                reg |= miu.page_mode << 6;
                return reg;
            }
            case 0x11E:
                return miu.mmio_base;
            case 0x182:
                return 0;
            case 0x184:
                return dma.chan_enable;
            case 0x186:
                return dma.arm_addr;
            case 0x18C:
                //End of transfer flags
                return 0xFFFF;
            case 0x1BE:
                return dma.channel;
            case 0x1DA:
                return dma.src_space[dma.channel] | (dma.dest_space[dma.channel] << 4);
            case 0x1DC:
                return 0;
            case 0x200:
                return icu.int_pending;
            case 0x202:
                return 0;
            case 0x204:
                return 0; //TODO: return int_pending?
            case 0x206:
                return icu.int_connection[0];
            case 0x208:
                return icu.int_connection[1];
            case 0x20A:
                return icu.int_connection[2];
            case 0x20C:
                return icu.vectored_int_connection;
            case 0x20E:
                return icu.int_mode;
            case 0x210:
                return icu.int_polarity;
            case 0x280:
                return 0; //TODO: BTDMP IRQ for receive enable
            case 0x2A0:
                return btdmp.irq_on_empty_transmit << 8;
            case 0x2C2:
            {
                uint16_t value = 0;
                value |= (btdmp.transmit_queue.size() == 16) << 3;
                value |= (btdmp.transmit_queue.size() == 0) << 4;
                return value;
            }
            case 0x2CA:
                return 0; //TODO: some sort of BTDMP wait flag?
            default:
                EmuException::die("[DSP] Unrecognized MMIO read $%04X\n", addr);
                return 0;
        }
    }

    uint32_t real_addr = convert_addr(addr);

    return *(uint16_t*)&dsp_mem[real_addr];
}

uint16_t DSP::read_from_page(uint8_t imm)
{
    return read_data_word((st1.page << 8) + imm);
}

uint16_t DSP::read_data_r7s(int16_t imm)
{
    return read_data_word(r[7] + imm);
}

uint16_t DSP::read_data_r16()
{
    return read_data_word(r[7] + fetch_code_word());
}

void DSP::write_data_word(uint16_t addr, uint16_t value)
{
    //printf("[DSP] Write data word $%04X: $%04X\n", addr, value);
    if (addr >= miu.mmio_base && addr < miu.mmio_base + 0x800)
    {
        addr &= 0x7FF;
        if (addr >= 0x0E2 && addr < 0x0E2 + (4 * 6))
        {
            int index = (addr - 0x0E2) / 6;
            int reg = (addr - 0x0E2) % 6;

            switch (reg)
            {
                case 0:
                    ahbm.burst[index] = (value >> 1) & 0x3;
                    ahbm.data_type[index] = (value >> 4) & 0x3;
                    break;
                case 2:
                    ahbm.transfer_dir[index] = (value >> 8) & 0x1;
                    break;
                case 4:
                    ahbm.chan_connection[index] = value & 0xFF;
                    break;
                default:
                    EmuException::die("[DSP_AHBM] Unrecognized write16 reg %d\n", reg);
            }
            return;
        }
        if (addr >= 0x212 && addr < 0x212 + (16 * 4))
        {
            int id = (addr - 0x212) / 4;
            int reg = (addr - 0x212) % 4;

            switch (reg)
            {
                case 0:
                    printf("[DSP_ICU] Write VINT_HI%d: $%04X\n", id, value);
                    icu.vector_ctx_switch[id] = (value >> 15) & 0x1;
                    icu.vector_addr[id] &= 0xFFFF;
                    icu.vector_addr[id] |= (value & 0x3) << 16;
                    break;
                case 2:
                    printf("[DSP_ICU] Write VINT_LO%d: $%04X\n", id, value);
                    icu.vector_addr[id] &= ~0xFFFF;
                    icu.vector_addr[id] |= value;
                    break;
            }
            return;
        }
        switch (addr)
        {
            case 0x020:
            case 0x030:
            {
                int index = (addr - 0x20) / 0x10;
                printf("[DSP_TIMER%d] Write CTRL: $%04X\n", index, value);

                timers[index].prescalar = value & 0x3;
                timers[index].countup_mode = (value >> 2) & 0x7;
                timers[index].enabled = (value >> 9) & 0x1;

                if (value & (1 << 10))
                {
                    timers[index].counter = timers[index].restart_value;
                }
            }
                break;
            case 0x024:
                printf("[DSP_TIMER0] Write RESTART_L: $%04X\n", value);
                timers[0].restart_value &= ~0xFFFF;
                timers[0].restart_value |= value;
                break;
            case 0x026:
                printf("[DSP_TIMER0] Write RESTART_H: $%04X\n", value);
                timers[0].restart_value &= 0xFFFF;
                timers[0].restart_value |= value << 16;
                break;
            case 0x034:
                printf("[DSP_TIMER1] Write RESTART_L: $%04X\n", value);
                timers[1].restart_value &= ~0xFFFF;
                timers[1].restart_value |= value;
                break;
            case 0x036:
                printf("[DSP_TIMER1] Write RESTART_H: $%04X\n", value);
                timers[1].restart_value &= 0xFFFF;
                timers[1].restart_value |= value << 16;
                break;
            case 0x0C0:
                printf("[DSP_APBP] Write REPLY0: $%04X\n", value);
                apbp.reply[0] = value;
                apbp.reply_ready[0] = true;

                if (apbp.reply_int_enable[0])
                    send_arm_interrupt();
                break;
            case 0x0C4:
                printf("[DSP_APBP] Write REPLY1: $%04X\n", value);

                apbp.reply[1] = value;
                apbp.reply_ready[1] = true;

                if (apbp.reply_int_enable[1])
                    send_arm_interrupt();
                break;
            case 0x0C8:
                printf("[DSP_APBP] Write REPLY2: $%04X\n", value);
                apbp.reply[2] = value;
                apbp.reply_ready[2] = true;

                if (apbp.reply_int_enable[2])
                    send_arm_interrupt();
                break;
            case 0x0CC:
                printf("[DSP_APBP] Write CPU_SEMA_RECV: $%04X\n", value);
            {
                uint16_t old_sema = apbp.cpu_sema_recv;
                uint16_t mask = ~apbp.cpu_sema_mask;

                if (!(old_sema & mask) && ((old_sema | value) & mask))
                    send_arm_interrupt();

                apbp.cpu_sema_recv = value;
            }
                break;
            case 0x0D0:
                printf("[DSP_APBP] Write DSP_SEMA_ACK: $%04X\n", value);
                apbp.dsp_sema_recv &= ~value;
                break;
            case 0x10E:
                miu.xpage = value & 0xFF;
                if (miu.xpage >= 2)
                    EmuException::die("[DSP] MIU XPAGE is greater than 1 ($%02X)", miu.xpage);
                break;
            case 0x110:
                miu.ypage = value & 0x0F;
                if (miu.ypage >= 2)
                    EmuException::die("[DSP] MIU YPAGE is greater than 1 ($%02X)", miu.ypage);
                break;
            case 0x114:
                printf("[DSP_MIU] Write X/YPAGE0CFG: $%04X\n", value);
                miu.x_size[0] = value & 0x3F;
                miu.y_size[0] = (value >> 8) & 0x7F;
                break;
            case 0x11A:
                miu.zsp = (value >> 4) & 0x1;
                miu.page_mode = (value >> 6) & 0x1;
                break;
            case 0x11E:
                miu.mmio_base = value & ~0x1FF;
                break;
            case 0x184:
                printf("[DSP_DMA] Write chan enable: $%04X\n", value);
                dma.chan_enable = value & 0xFF;
                break;
            case 0x1BE:
                printf("[DSP_DMA] Write chan: $%04X\n", value);
                dma.channel = value & 0x7;
                break;
            case 0x1C4:
                printf("[DSP_DMA] Write DST_ADDR_LOW_%d: $%04X\n", dma.channel, value);
                dma.dest_addr[dma.channel] &= ~0xFFFF;
                dma.dest_addr[dma.channel] |= value;
                break;
            case 0x1C6:
                printf("[DSP_DMA] Write DST_ADDR_HIGH_%d: $%04X\n", dma.channel, value);
                dma.dest_addr[dma.channel] &= 0xFFFF;
                dma.dest_addr[dma.channel] |= value << 16;
                break;
            case 0x1C8:
                printf("[DSP_DMA] Write SIZE0_%d: $%04X\n", dma.channel, value);
                dma.size[0][dma.channel] = value;
                break;
            case 0x1CA:
                printf("[DSP_DMA] Write SIZE1_%d: $%04X\n", dma.channel, value);
                dma.size[1][dma.channel] = value;
                break;
            case 0x1CC:
                printf("[DSP_DMA] Write SIZE2_%d: $%04X\n", dma.channel, value);
                dma.size[2][dma.channel] = value;
                break;
            case 0x1CE:
                printf("[DSP_DMA] Write SRC_STEP0_%d: $%04X\n", dma.channel, value);
                dma.src_step[0][dma.channel] = value;
                break;
            case 0x1D0:
                printf("[DSP_DMA] Write DST_STEP0_%d: $%04X\n", dma.channel, value);
                dma.dest_step[0][dma.channel] = value;
                break;
            case 0x1D2:
                printf("[DSP_DMA] Write SRC_STEP1_%d: $%04X\n", dma.channel, value);
                dma.src_step[1][dma.channel] = value;
                break;
            case 0x1D4:
                printf("[DSP_DMA] Write DST_STEP1_%d: $%04X\n", dma.channel, value);
                dma.dest_step[1][dma.channel] = value;
                break;
            case 0x1D6:
                printf("[DSP_DMA] Write SRC_STEP2_%d: $%04X\n", dma.channel, value);
                dma.src_step[2][dma.channel] = value;
                break;
            case 0x1D8:
                printf("[DSP_DMA] Write DST_STEP2_%d: $%04X\n", dma.channel, value);
                dma.dest_step[2][dma.channel] = value;
                break;
            case 0x1DA:
                printf("[DSP_DMA] Write 0x1DA_%d: $%04X\n", dma.channel, value);
                dma.src_space[dma.channel] = value & 0xF;
                dma.dest_space[dma.channel] = (value >> 4) & 0xF;
                break;
            case 0x1DC:
                printf("[DSP_DMA] Write 0x1DC: $%04X\n", value);
                break;
            case 0x202:
                printf("[DSP_ICU] Write int acknowledge: $%04X\n", value);
                icu.int_pending &= ~value;

                //Disable int pending signals on STT2
                for (int i = 0; i < 16; i++)
                {
                    if (!(value & (1 << i)))
                        continue;

                    for (int j = 0; j < 3; j++)
                    {
                        if (icu.int_connection[j] & (1 << i))
                            stt2.int_pending[j] = false;
                    }

                    if (icu.vectored_int_connection & (1 << i))
                        stt2.vectored_int_pending = false;
                }
                break;
            case 0x204:
                printf("[DSP_ICU] Write SWI: $%04X\n", value);
                for (int i = 0; i < 16; i++)
                {
                    if (value & (1 << i))
                        assert_dsp_irq(i);
                }
                break;
            case 0x206:
                printf("[DSP_ICU] Write int0 connection: $%04X\n", value);
                icu.int_connection[0] = value;
                break;
            case 0x208:
                printf("[DSP_ICU] Write int1 connection: $%04X\n", value);
                icu.int_connection[1] = value;
                break;
            case 0x20A:
                printf("[DSP_ICU] Write int2 connection: $%04X\n", value);
                icu.int_connection[2] = value;
                break;
            case 0x20C:
                printf("[DSP_ICU] Write vint connection: $%04X\n", value);
                icu.vectored_int_connection = value;
                break;
            case 0x20E:
                icu.int_mode = value;
                break;
            case 0x210:
                icu.int_polarity = value;
                break;
            case 0x280:
            case 0x282:
            case 0x284:
            case 0x286:
            case 0x288:
            case 0x28A:
            case 0x28C:
            case 0x29E:
                break;
            case 0x2A0:
                btdmp.irq_on_empty_transmit = (value >> 8) & 0x1;
                break;
            case 0x2A2:
                btdmp.cycles_per_transmit = value;
                break;
            case 0x2A4:
            case 0x2A6:
            case 0x2A8:
            case 0x2AA:
            case 0x2AC:
                break;
            case 0x2BE:
                btdmp.transmit_enabled = value >> 15;
                if (btdmp.transmit_enabled)
                    btdmp.transmit_cycles_left = btdmp.cycles_per_transmit;
                break;
            case 0x2C6:
                btdmp.transmit_queue.push(value);
                break;
            case 0x2CA:
                break;
            default:
                EmuException::die("[DSP] Unrecognized MMIO write $%04X: $%04X\n", addr, value);
        }
    }
    else
    {
        uint32_t real_addr = convert_addr(addr);

        *(uint16_t*)&dsp_mem[real_addr] = value;
    }
}

void DSP::write_to_page(uint8_t imm, uint16_t value)
{
    write_data_word((st1.page << 8) + imm, value);
}

void DSP::write_data_r7s(int16_t imm, uint16_t value)
{
    write_data_word(r[7] + imm, value);
}

bool DSP::meets_condition(uint8_t cond)
{
    switch (cond)
    {
        case 0x00:
            //always
            return true;
        case 0x01:
            //eq
            return stt0.fz;
        case 0x02:
            //ne
            return !stt0.fz;
        case 0x03:
            //gt
            return !stt0.fz && !stt0.fm;
        case 0x04:
            //ge
            return !stt0.fm;
        case 0x05:
            //lt
            return stt0.fm;
        case 0x06:
            //le
            return stt0.fm || stt0.fz;
        case 0x0C:
            //nr
            return !st0.fr;
        default:
            EmuException::die("[DSP] Unrecognized condition $%02X", cond);
            return false;
    }
}

uint64_t DSP::get_acc(DSP_REG acc)
{
    switch (acc)
    {
        case DSP_REG_A0:
        case DSP_REG_A0l:
        case DSP_REG_A0h:
        case DSP_REG_A0e:
            return a0;
        case DSP_REG_A1:
        case DSP_REG_A1l:
        case DSP_REG_A1h:
        case DSP_REG_A1e:
            return a1;
        case DSP_REG_B0:
        case DSP_REG_B0l:
        case DSP_REG_B0h:
        case DSP_REG_B0e:
            return b0;
        case DSP_REG_B1:
        case DSP_REG_B1l:
        case DSP_REG_B1h:
        case DSP_REG_B1e:
            return b1;
        default:
            EmuException::die("[DSP] Unrecognized acc %d in get_acc", acc);
    }
    return 0;
}

uint32_t DSP::get_product_no_shift(int index)
{
    return p[index];
}

uint64_t DSP::get_product(int index)
{
    uint64_t value = p[index] | ((uint64_t)pe[index] << 32ULL);

    uint8_t ps;
    if (index == 0)
        ps = st1.PS;
    else
        ps = mod0.PS1;

    switch (ps)
    {
        case 0:
            value = SignExtend<33>(value);
            break;
        case 1:
            value >>= 1;
            value = SignExtend<32>(value);
            break;
        case 2:
            value <<= 1;
            value = SignExtend<34>(value);
            break;
        case 3:
            value <<= 2;
            value = SignExtend<35>(value);
            break;
    }

    return value;
}

uint64_t DSP::get_add_sub_result(uint64_t a, uint64_t b, bool is_sub)
{
    //Zero extend to 40 bits
    a = trunc_to_40(a);
    b = trunc_to_40(b);

    uint64_t result;
    if (is_sub)
    {
        result = a - b;
        b = ~b;
    }
    else
        result = a + b;

    stt0.fc = (result >> 40) & 0x1;
    stt0.fv = ((~(a ^ b) & (a ^ result)) >> 39) & 1;
    if (stt0.fv)
        stt0.fvl = true;
    return SignExtend<40>(result);
}

uint16_t DSP::get_reg16(DSP_REG reg, bool mov_saturate)
{
    switch (reg)
    {
        case DSP_REG_R0:
            return r[0];
        case DSP_REG_R1:
            return r[1];
        case DSP_REG_R2:
            return r[2];
        case DSP_REG_R3:
            return r[3];
        case DSP_REG_R4:
            return r[4];
        case DSP_REG_R5:
            return r[5];
        case DSP_REG_R6:
            return r[6];
        case DSP_REG_R7:
            return r[7];
        case DSP_REG_Y0:
            return y[0];
        case DSP_REG_P:
            //TODO: This only happens for instructions with the Register operand.
            return (get_product(0) >> 16) & 0xFFFF;
        case DSP_REG_A0l:
        case DSP_REG_A1l:
        case DSP_REG_B0l:
        case DSP_REG_B1l:
            if (mov_saturate)
                return get_saturated_acc(reg) & 0xFFFF;
            return get_acc(reg) & 0xFFFF;
        case DSP_REG_A0h:
        case DSP_REG_A1h:
        case DSP_REG_B0h:
        case DSP_REG_B1h:
            if (mov_saturate)
                return (get_saturated_acc(reg) >> 16) & 0xFFFF;
            return (get_acc(reg) >> 16) & 0xFFFF;
        case DSP_REG_SP:
            return sp;
        case DSP_REG_CFGI:
            return stepi | (modi << 7);
        case DSP_REG_CFGJ:
            return stepj | (modj << 7);
        case DSP_REG_AR0:
            return get_ar(0);
        case DSP_REG_AR1:
            return get_ar(1);
        case DSP_REG_ARP0:
            return get_arp(0);
        case DSP_REG_ARP1:
            return get_arp(1);
        case DSP_REG_ARP2:
            return get_arp(2);
        case DSP_REG_ARP3:
            return get_arp(3);
        case DSP_REG_ST0:
        {
            uint16_t value = 0;
            value |= mod0.sat;
            value |= mod3.master_int_enable << 1;
            value |= mod3.int_enable[0] << 2;
            value |= mod3.int_enable[1] << 3;
            value |= st0.fr << 4;
            value |= stt0.flm << 5;
            value |= stt0.fe << 6;
            value |= stt0.fc << 7;
            value |= stt0.fv << 8;
            value |= stt0.fn << 9;
            value |= stt0.fm << 10;
            value |= stt0.fz << 11;
            value |= ((a0 >> 32) & 0xF) << 12;
            return value;
        }
        case DSP_REG_ST1:
        {
            uint16_t value = 0;
            value |= st1.page;
            value |= st1.PS << 10;
            value |= ((a1 >> 32) & 0xF) << 12;
            return value;
        }
        case DSP_REG_STT0:
        {
            uint16_t value = 0;
            value |= stt0.flm;
            value |= stt0.fvl << 1;
            value |= stt0.fe << 2;
            value |= stt0.fc << 3;
            value |= stt0.fv << 4;
            value |= stt0.fn << 5;
            value |= stt0.fm << 6;
            value |= stt0.fz << 7;
            value |= stt0.fc1 << 11;
            return value;
        }
        case DSP_REG_STT1:
        {
            uint16_t value = 0;
            value |= st0.fr << 4;
            value |= (pe[0] & 0x1) << 14;
            value |= (pe[1] & 0x1) << 15;
            return value;
        }
        case DSP_REG_STT2:
        {
            uint16_t value = 0;
            value |= stt2.int_pending[0];
            value |= stt2.int_pending[1] << 1;
            value |= stt2.int_pending[2] << 2;
            value |= stt2.vectored_int_pending << 3;
            value |= stt2.pcmhi << 6;
            value |= stt2.bcn << 12;
            value |= stt2.lp << 15;
            return value;
        }
        case DSP_REG_MOD0:
        {
            uint16_t value = 0;
            value |= mod0.sat;
            value |= mod0.sata << 1;
            value |= 1 << 2;
            value |= mod0.hwm << 5;
            value |= st2.s << 7;
            value |= st1.PS << 10;
            value |= mod0.PS1 << 13;
            return value;
        }
        case DSP_REG_MOD1:
        {
            uint16_t value = 0;
            value |= st1.page;
            value |= mod1.stp16 << 12;
            value |= mod1.cmd << 13;
            return value;
        }
        case DSP_REG_MOD2:
        {
            uint16_t value = 0;
            for (int i = 0; i < 8; i++)
            {
                value |= mod2.m[i] << i;
                value |= mod2.br[i] << (i + 8);
            }
            return value;
        }
        case DSP_REG_MOD3:
        {
            uint16_t value = 0;
            value |= mod3.nmi_ctx_switch;
            value |= mod3.int_ctx_switch[0] << 1;
            value |= mod3.int_ctx_switch[1] << 2;
            value |= mod3.int_ctx_switch[2] << 3;

            value |= mod3.master_int_enable << 7;
            value |= mod3.int_enable[0] << 8;
            value |= mod3.int_enable[1] << 9;
            value |= mod3.int_enable[2] << 10;
            value |= mod3.vectored_int_enable << 11;

            value |= mod3.ccnta << 13;
            value |= mod3.reverse_stack_order << 14;
            value |= mod3.crep << 15;
            return value;
        }
        case DSP_REG_LC:
            if (stt2.lp)
                return bkrep_stack[stt2.bcn - 1].lc;
            return bkrep_stack[0].lc;
        case DSP_REG_SV:
            return sv;
        default:
            EmuException::die("[DSP] Unrecognized reg %d in get_reg16", reg);
            return 0;
    }
}

uint64_t DSP::get_saturated_acc(DSP_REG reg)
{
    uint64_t acc = get_acc(reg);
    if (!mod0.sat)
        return saturate(acc);
    return acc;
}

void DSP::set_reg16(DSP_REG reg, uint16_t value)
{
    switch (reg)
    {
        case DSP_REG_R0:
            r[0] = value;
            break;
        case DSP_REG_R1:
            r[1] = value;
            break;
        case DSP_REG_R2:
            r[2] = value;
            break;
        case DSP_REG_R3:
            r[3] = value;
            break;
        case DSP_REG_R4:
            r[4] = value;
            break;
        case DSP_REG_R5:
            r[5] = value;
            break;
        case DSP_REG_R6:
            r[6] = value;
            break;
        case DSP_REG_R7:
            r[7] = value;
            break;
        case DSP_REG_Y0:
            y[0] = value;
            break;
        case DSP_REG_P:
            pe[0] = value > 0x7FFF;
            p[0] &= 0xFFFF;
            p[0] |= value << 16;
            break;
        case DSP_REG_A0:
        case DSP_REG_A1:
        case DSP_REG_B0:
        case DSP_REG_B1:
            saturate_acc_with_flag(reg, SignExtend<16, uint64_t>(value));
            break;
        case DSP_REG_A0l:
        case DSP_REG_A1l:
        case DSP_REG_B0l:
        case DSP_REG_B1l:
            saturate_acc_with_flag(reg, value);
            break;
        case DSP_REG_A0h:
        case DSP_REG_A1h:
        case DSP_REG_B0h:
        case DSP_REG_B1h:
            saturate_acc_with_flag(reg, SignExtend<32, uint64_t>(value << 16));
            break;
        case DSP_REG_SP:
            sp = value;
            break;
        case DSP_REG_CFGI:
            stepi = value & 0x7F;
            modi = value >> 7;
            break;
        case DSP_REG_CFGJ:
            stepj = value & 0x7F;
            modj = value >> 7;
            break;
        case DSP_REG_AR0:
            set_ar(0, value);
            break;
        case DSP_REG_AR1:
            set_ar(1, value);
            break;
        case DSP_REG_ARP0:
            set_arp(0, value);
            break;
        case DSP_REG_ARP1:
            set_arp(1, value);
            break;
        case DSP_REG_ARP2:
            set_arp(2, value);
            break;
        case DSP_REG_ARP3:
            set_arp(3, value);
            break;
        case DSP_REG_ST0:
            mod0.sat = value & 0x1;
            mod3.master_int_enable = (value >> 1) & 0x1;
            mod3.int_enable[0] = (value >> 2) & 0x1;
            mod3.int_enable[1] = (value >> 3) & 0x1;
            st0.fr = (value >> 4) & 0x1;
            stt0.flm = (value >> 5) & 0x1;
            stt0.fvl = (value >> 5) & 0x1;
            stt0.fe = (value >> 6) & 0x1;
            stt0.fc = (value >> 7) & 0x1;
            stt0.fv = (value >> 8) & 0x1;
            stt0.fn = (value >> 9) & 0x1;
            stt0.fm = (value >> 10) & 0x1;
            stt0.fz = (value >> 11) & 0x1;

            a0 &= 0xFFFFFFFF;
            a0 |= ((value >> 12) & 0xFULL) << 32ULL;
            break;
        case DSP_REG_ST1:
            st1.page = value & 0xFF;
            st1.PS = (value >> 10) & 0x3;

            a1 &= 0xFFFFFFFF;
            a1 |= ((value >> 12) & 0xFULL) << 32ULL;
            break;
        case DSP_REG_ST2:
            printf("[DSP] Write16 ST2: $%04X\n", value);
            st2.s = (value >> 7) & 0x1;
            break;
        case DSP_REG_STT0:
            stt0.flm = value & 0x1;
            stt0.fvl = (value >> 1) & 0x1;
            stt0.fe = (value >> 2) & 0x1;
            stt0.fc = (value >> 3) & 0x1;
            stt0.fv = (value >> 4) & 0x1;
            stt0.fn = (value >> 5) & 0x1;
            stt0.fm = (value >> 6) & 0x1;
            stt0.fz = (value >> 7) & 0x1;
            stt0.fc1 = (value >> 11) & 0x1;
            break;
        case DSP_REG_STT1:
            st0.fr = (value >> 4) & 0x1;

            //TODO: pe writes?
            break;
        case DSP_REG_STT2:
            stt2.pcmhi = (value >> 6) & 0x3;
            break;
        case DSP_REG_MOD0:
            mod0.sat = value & 0x1;
            mod0.sata = (value >> 1) & 0x1;
            mod0.hwm = (value >> 5) & 0x3;
            st2.s = (value >> 7) & 0x1;
            st1.PS = (value >> 10) & 0x3;
            mod0.PS1 = (value >> 13) & 0x3;
            break;
        case DSP_REG_MOD1:
            st1.page = value & 0xFF;
            mod1.stp16 = (value >> 12) & 0x1;
            mod1.cmd = (value >> 13) & 0x1;
            mod1.epi = (value >> 14) & 0x1;
            mod1.epj = (value >> 15) & 0x1;
            break;
        case DSP_REG_MOD2:
            for (int i = 0; i < 8; i++)
            {
                mod2.m[i] = (value >> i) & 0x1;
                mod2.br[i] = (value >> (i + 8)) & 0x1;
            }
            break;
        case DSP_REG_MOD3:
            mod3.nmi_ctx_switch = value & 0x1;
            mod3.int_ctx_switch[0] = (value >> 1) & 0x1;
            mod3.int_ctx_switch[1] = (value >> 2) & 0x1;
            mod3.int_ctx_switch[2] = (value >> 3) & 0x1;

            mod3.master_int_enable = (value >> 7) & 0x1;
            mod3.int_enable[0] = (value >> 8) & 0x1;
            mod3.int_enable[1] = (value >> 9) & 0x1;
            mod3.int_enable[2] = (value >> 10) & 0x1;
            mod3.vectored_int_enable = (value >> 11) & 0x1;

            mod3.ccnta = (value >> 13) & 0x1;
            mod3.reverse_stack_order = (value >> 14) & 0x1;
            mod3.crep = (value >> 15) & 0x1;
            break;
        case DSP_REG_LC:
            if (stt2.lp)
                bkrep_stack[stt2.bcn - 1].lc = value;
            bkrep_stack[0].lc = value;
            break;
        case DSP_REG_SV:
            sv = value;
            break;
        default:
            EmuException::die("[DSP] Unrecognized reg %d in set_reg16 (value: $%04X)", reg, value);
    }
}

void DSP::set_acc_and_flag(DSP_REG acc, uint64_t value)
{
    set_acc_flags(value);
    set_acc(acc, value);
}

void DSP::saturate_acc_with_flag(DSP_REG acc, uint64_t value)
{
    set_acc_flags(value);
    if (!mod0.sata)
        value = saturate(value);

    set_acc(acc, value);
}

uint64_t DSP::trunc_to_40(uint64_t value)
{
    return value & 0xFFFFFFFFFFULL;
}

uint64_t DSP::saturate(uint64_t value)
{
    if (value != SignExtend<32>(value))
    {
        stt0.flm = true;
        if ((value >> 39) & 0x1)
            return 0xFFFFFFFF80000000ULL;
        else
            return 0x000000007FFFFFFFULL;
    }
    return value;
}

void DSP::set_acc_lo(DSP_REG acc, uint16_t value)
{
    switch (acc)
    {
        case DSP_REG_A0l:
            a0 = trunc_to_40(a0);
            a0 &= ~0xFFFFULL;
            a0 |= value;
            break;
        case DSP_REG_A1l:
            a1 = trunc_to_40(a1);
            a1 &= ~0xFFFFULL;
            a1 |= value;
            break;
        case DSP_REG_B0l:
            b0 = trunc_to_40(b0);
            b0 &= ~0xFFFFULL;
            b0 |= value;
            break;
        case DSP_REG_B1l:
            b1 = trunc_to_40(b1);
            b1 &= ~0xFFFFULL;
            b1 |= value;
            break;
        default:
            EmuException::die("[DSP] Unrecognized acc %d in set_acc_lo", acc);
    }
}

void DSP::set_acc_hi(DSP_REG acc, uint16_t value)
{
    switch (acc)
    {
        case DSP_REG_A0h:
            a0 = trunc_to_40(a0);
            a0 &= ~0xFFFF0000ULL;
            a0 |= value << 16;
            break;
        case DSP_REG_A1h:
            a1 = trunc_to_40(a1);
            a1 &= ~0xFFFF0000ULL;
            a1 |= value << 16;
            break;
        case DSP_REG_B0h:
            b0 = trunc_to_40(b0);
            b0 &= ~0xFFFF0000ULL;
            b0 |= value << 16;
            break;
        case DSP_REG_B1h:
            b1 = trunc_to_40(b1);
            b1 &= ~0xFFFF0000ULL;
            b1 |= value << 16;
            break;
        default:
            EmuException::die("[DSP] Unrecognized acc %d in set_acc_hi", acc);
    }
}

void DSP::set_acc_flags(uint64_t value)
{
    stt0.fz = value == 0;
    stt0.fm = (value >> 39) & 0x1;
    stt0.fe = value != SignExtend<32>(value);

    bool bit31 = (value >> 31) & 0x1;
    bool bit30 = (value >> 30) & 0x1;

    stt0.fn = stt0.fz || (!stt0.fe && (bit31 ^ bit30) != 0);
}

void DSP::set_acc(DSP_REG acc, uint64_t value)
{
    switch (acc)
    {
        case DSP_REG_A0:
        case DSP_REG_A0l:
        case DSP_REG_A0h:
        case DSP_REG_A0e:
            a0 = value;
            break;
        case DSP_REG_A1:
        case DSP_REG_A1l:
        case DSP_REG_A1h:
        case DSP_REG_A1e:
            a1 = value;
            break;
        case DSP_REG_B0:
        case DSP_REG_B0l:
        case DSP_REG_B0h:
        case DSP_REG_B0e:
            b0 = value;
            break;
        case DSP_REG_B1:
        case DSP_REG_B1l:
        case DSP_REG_B1h:
        case DSP_REG_B1e:
            b1 = value;
            break;
        default:
            EmuException::die("[DSP] Unrecognized acc register %d in set_acc (value: %llX)", acc, value);
    }
}

void DSP::set_product(int index, uint32_t value)
{
    p[index] = value;
    pe[index] = value >> 31;
}

void DSP::push16(uint16_t value)
{
    sp--;
    write_data_word(sp, value);
}

uint16_t DSP::pop16()
{
    uint16_t value = read_data_word(sp);
    sp++;
    return value;
}

void DSP::push_pc()
{
    uint16_t low = pc & 0xFFFF;
    uint16_t hi = pc >> 16;

    if (mod3.reverse_stack_order)
    {
        write_data_word(sp - 1, hi);
        write_data_word(sp - 2, low);
    }
    else
    {
        write_data_word(sp - 1, low);
        write_data_word(sp - 2, hi);
    }

    sp -= 2;
}

void DSP::pop_pc()
{
    uint16_t low, hi;

    if (mod3.reverse_stack_order)
    {
        low = read_data_word(sp);
        hi = read_data_word(sp + 1);
    }
    else
    {
        hi = read_data_word(sp);
        low = read_data_word(sp + 1);
    }
    sp += 2;

    pc = low | ((uint32_t)hi << 16);
}

void DSP::save_shadows()
{
    stt0s = stt0;
    st0s = st0;
}

void DSP::swap_shadows()
{
    //TODO: check this function!
    std::swap(st0, st0s);
    std::swap(st1, st1s);
    std::swap(st2, st2s);
    std::swap(stt0, stt0s);
    //std::swap(stt1, stt1s);
    std::swap(stt2.pcmhi, stt2s.pcmhi);
    std::swap(mod0, mod0s);
    std::swap(mod1, mod1s);
    std::swap(mod2, mod2s);

    //TODO: swap mod3 int0/int1/int2/vint enable?
    std::swap(mod3.int_enable[0], mod3s.int_enable[0]);
    std::swap(mod3.int_enable[1], mod3s.int_enable[1]);
    std::swap(mod3.int_enable[2], mod3s.int_enable[2]);
    std::swap(mod3.vectored_int_enable, mod3s.vectored_int_enable);

    swap_all_ararps();
}

void DSP::swap_all_ararps()
{
    std::swap(ar[0], ars[0]);
    std::swap(ar[1], ars[1]);
    std::swap(arp[0], arps[0]);
    std::swap(arp[1], arps[1]);
    std::swap(arp[2], arps[2]);
    std::swap(arp[3], arps[3]);
}

void DSP::restore_shadows()
{
    stt0 = stt0s;
    st0 = st0s;
}

void DSP::save_context()
{
    save_shadows();
    swap_shadows();
    if (!mod3.crep)
        repcs = repc;

    if (!mod3.ccnta)
    {
        a1s = a1;
        b1s = b1;
    }
    else
    {
        //We do a manual swap because b1->a1 sets flags
        uint64_t a = a1;
        uint64_t b = b1;

        b1 = a;
        set_acc_and_flag(DSP_REG_A1, b);
    }
}

void DSP::restore_context()
{
    restore_shadows();
    swap_shadows();

    if (!mod3.crep)
        repc = repcs;

    if (!mod3.ccnta)
    {
        a1 = a1s;
        b1 = b1s;
    }
    else
        std::swap(a1, b1);
}

void DSP::banke(uint8_t flags)
{
    if (flags & 0x01)
    {
        //CFGI
        std::swap(stepi, stepib);
        std::swap(modi, modib);
        if (mod1.stp16)
            std::swap(stepi0, stepi0b);
    }

    if (flags & 0x02)
    {
        //R4
        std::swap(r[4], r4b);
    }

    if (flags & 0x04)
    {
        //R1
        std::swap(r[1], r1b);
    }

    if (flags & 0x08)
    {
        //R0
        std::swap(r[0], r0b);
    }

    if (flags & 0x10)
    {
        //R7
        std::swap(r[7], r7b);
    }

    if (flags & 0x20)
    {
        //CFGJ
        std::swap(stepj, stepjb);
        std::swap(modj, modjb);
        if (mod1.stp16)
            std::swap(stepj0, stepj0b);
    }
}

void DSP::break_()
{
    if (!stt2.lp)
        EmuException::die("[DSP] break executed when not in block repeat loop!");
    stt2.bcn--;
    stt2.lp = stt2.bcn > 0;
}

void DSP::repeat(uint16_t lc)
{
    repc = lc;
    rep = true;
}

void DSP::block_repeat(uint16_t lc, uint32_t end_addr)
{
    if (stt2.bcn > 3)
        EmuException::die("[DSP] block_repeat called when bcn > 3!");

    bkrep_stack[stt2.bcn].end = end_addr;
    bkrep_stack[stt2.bcn].start = pc;
    bkrep_stack[stt2.bcn].lc = lc;
    stt2.bcn++;
    stt2.lp = true;
}

uint16_t DSP::restore_block_repeat(uint16_t addr)
{
    if (stt2.lp)
    {
        //EmuException::die("[DSP] Verify restore_block_repeat!");
        DSP_BKREP_ELEMENT temp[4];

        for (int i = 0; i < stt2.bcn; i++)
        {
            temp[i].end = bkrep_stack[i].end;
            temp[i].start = bkrep_stack[i].start;
            temp[i].lc = bkrep_stack[i].lc;
        }

        for (int i = 0; i < stt2.bcn; i++)
        {
            bkrep_stack[i + 1].end = temp[i].end;
            bkrep_stack[i + 1].start = temp[i].start;
            bkrep_stack[i + 1].lc = temp[i].lc;
        }

        stt2.bcn++;
    }

    uint16_t flag = read_data_word(addr);

    if (!stt2.lp)
    {
        if (flag >> 15)
            stt2.lp = stt2.bcn = 1;
    }

    bkrep_stack[0].end = read_data_word(addr + 1) | (((flag >> 8) & 0x3) << 16);
    bkrep_stack[0].start = read_data_word(addr + 2) | ((flag & 0x3) << 16);
    bkrep_stack[0].lc = read_data_word(addr + 3);

    addr += 4;
    return addr;
}

uint16_t DSP::store_block_repeat(uint16_t addr)
{
    write_data_word(addr - 1, bkrep_stack[0].lc);
    write_data_word(addr - 2, bkrep_stack[0].start & 0xFFFF);
    write_data_word(addr - 3, bkrep_stack[0].end & 0xFFFF);

    uint16_t flag = stt2.lp << 15;
    flag |= bkrep_stack[0].start >> 16;
    flag |= (bkrep_stack[0].end >> 16) << 8;
    write_data_word(addr - 4, flag);
    addr -= 4;

    if (stt2.lp)
    {
        for (int i = 0; i < stt2.bcn - 1; i++)
        {
            bkrep_stack[i].end = bkrep_stack[i + 1].end;
            bkrep_stack[i].start = bkrep_stack[i + 1].start;
            bkrep_stack[i].lc = bkrep_stack[i + 1].lc;
        }
        stt2.bcn--;
        if (stt2.bcn == 0)
            stt2.lp = false;
    }

    return addr;
}

uint16_t DSP::exp(uint64_t value)
{
    bool sign = (value >> 39) & 0x1;
    uint16_t bit = 38, count = 0;
    while (true)
    {
        if (((value >> bit) & 1) != sign)
            break;
        count++;
        if (bit == 0)
            break;
        bit--;
    }
    return count - 8;
}

void DSP::shift_reg_40(uint64_t value, DSP_REG dest, uint16_t shift)
{
    value = trunc_to_40(value);

    uint64_t original_sign = value >> 39;
    if ((shift >> 15) == 0)
    {
        // left shift
        if (shift >= 40)
        {
            if (st2.s == 0)
            {
                stt0.fv = value != 0;
                if (stt0.fv)
                    stt0.fvl = 1;
            }
            value = 0;
            stt0.fc = 0;
        }
        else
        {
            if (st2.s == 0)
            {
                stt0.fv = SignExtend<40>(value) != SignExtend(value, 40 - shift);
                if (stt0.fv)
                    stt0.fvl = 1;
            }
            value <<= shift;
            stt0.fc = (value & ((uint64_t)1 << 40)) != 0;
        }
    }
    else
    {
        // right shift
        uint16_t nshift = ~shift + 1;
        if (nshift >= 40)
        {
            if (st2.s == 0)
            {
                stt0.fc = (value >> 39) & 1;
                value = stt0.fc ? 0xFFFFFFFFFFULL : 0;
            }
            else
            {
                value = 0;
                stt0.fc = 0;
            }
        }
        else
        {
            stt0.fc = (value & ((uint64_t)1 << (nshift - 1))) != 0;
            value >>= nshift;
            if (st2.s == 0)
                value = SignExtend(value, 40 - nshift);
        }

        if (st2.s == 0)
            stt0.fv = 0;
    }

    value = SignExtend<40>(value);
    set_acc_flags(value);
    if (st2.s == 0 && mod0.sata == 0)
    {
        if (stt0.fv || SignExtend<32>(value) != value)
        {
            stt0.flm = 1;
            value = original_sign == 1 ? 0xFFFFFFFF80000000ULL : 0x7FFFFFFFULL;
        }
    }
    set_acc(dest, value);
}

void DSP::multiply(uint32_t unit, bool x_sign, bool y_sign)
{
    uint32_t a = x[unit];
    uint32_t b = y[unit];

    switch (mod0.hwm)
    {
        case 0:
            //Do nothing, read value directly
            break;
        case 1:
            b >>= 8;
            break;
        case 2:
            b &= 0xFF;
            break;
        case 3:
            if (unit)
                b &= 0xFF;
            else
                b >>= 8;
            break;
    }

    if (x_sign)
        a = SignExtend<16>(a);
    if (y_sign)
        b = SignExtend<16>(b);

    p[unit] = a * b;
    //printf("MULTIPLY: $%04X * $%04X = $%08X\n", a, b, p[unit]);

    if (x_sign || y_sign)
        pe[unit] = p[unit] >> 31;
    else
        pe[unit] = 0;
}

void DSP::product_sum(int base, DSP_REG acc, bool sub_p0, bool p0_align, bool sub_p1, bool p1_align)
{
    uint64_t pa = get_product(0);
    uint64_t pb = get_product(1);

    if (p0_align)
        pa = SignExtend<24>(pa >> 16);
    if (p1_align)
        pb = SignExtend<24>(pb >> 16);

    uint64_t sum = 0;
    switch (base)
    {
        case 0:
            //Zero
            sum = 0;
            break;
        case 1:
            //Acc
            sum = get_acc(acc);
            break;
        case 2:
            //Sv
            sum = SignExtend<32, uint64_t>((uint64_t)sv << 16);
            break;
        case 3:
            //SvRnd
            sum = SignExtend<32, uint64_t>((uint64_t)sv << 16) | 0x8000;
            break;
    }

    uint64_t result = get_add_sub_result(sum, pa, sub_p0);

    bool fc = stt0.fc;
    bool fv = stt0.fv;

    result = get_add_sub_result(result, pb, sub_p1);

    //TODO: Is this correct?
    if (sub_p0 == sub_p1)
    {
        stt0.fc |= fc;
        stt0.fv |= fv;
    }
    else
    {
        stt0.fc ^= fc;
        stt0.fv ^= fv;
    }

    saturate_acc_with_flag(acc, result);
}

uint8_t DSP::get_arprni(uint8_t value)
{
    return arp[value].rn[0];
}

uint8_t DSP::get_arprnj(uint8_t value)
{
    return arp[value].rn[1] + 4;
}

uint8_t DSP::get_arpstepi(uint8_t value)
{
    return arp[value].step[0];
}

uint8_t DSP::get_arpstepj(uint8_t value)
{
    return arp[value].step[1];
}

uint8_t DSP::get_arpoffseti(uint8_t value)
{
    return arp[value].offset[0];
}

uint8_t DSP::get_arpoffsetj(uint8_t value)
{
    return arp[value].offset[1];
}

uint8_t DSP::get_arrn_unit(uint8_t value)
{
    uint8_t arrn = 0;
    switch (value)
    {
        case 0x0:
            arrn = ar[0].rn[0];
            break;
        case 0x1:
            arrn = ar[0].rn[1];
            break;
        case 0x2:
            arrn = ar[1].rn[0];
            break;
        case 0x3:
            arrn = ar[1].rn[1];
            break;
        default:
            EmuException::die("[DSP] Unrecognized arrn $%02X in get_arrn_unit", value);
    }
    return arrn;
}

uint8_t DSP::get_arstep(uint8_t value)
{
    uint8_t arstep = 0;
    switch (value)
    {
        case 0x0:
            arstep = ar[0].step[0];
            break;
        case 0x1:
            arstep = ar[0].step[1];
            break;
        case 0x2:
            arstep = ar[1].step[0];
            break;
        case 0x3:
            arstep = ar[1].step[1];
            break;
        default:
            EmuException::die("[DSP] Unrecognized arstep $%02X in get_arstep", value);
    }
    return arstep;
}

uint8_t DSP::get_aroffset(uint8_t value)
{
    uint8_t aroffset = 0;
    switch (value)
    {
        case 0x0:
            aroffset = ar[0].offset[0];
            break;
        case 0x1:
            aroffset = ar[0].offset[1];
            break;
        case 0x2:
            aroffset = ar[1].offset[0];
            break;
        case 0x3:
            aroffset = ar[1].offset[1];
            break;
        default:
            EmuException::die("[DSP] Unrecognized aroffset $%02X in get_aroffset", value);
    }
    return aroffset;
}

uint16_t DSP::rn_addr(uint8_t rn, uint16_t value)
{
    if (mod2.br[rn] && !mod2.m[rn])
        EmuException::die("[DSP] rn_addr requires bit reversal!");
    return value;
}

uint16_t DSP::rn_and_modify(uint8_t rn, uint8_t step, bool dmod)
{
    uint16_t value = r[rn];

    if ((rn == 3 && mod1.epi) || (rn == 7 && mod1.epi))
        EmuException::die("[DSP] rn_and_modify needs check for epi/epj");

    r[rn] = step_addr(rn, value, step, dmod);

    return value;
}

uint16_t DSP::rn_addr_and_modify(uint8_t rn, uint8_t step, bool dmod)
{
    return rn_addr(rn, rn_and_modify(rn, step, dmod));
}

uint16_t DSP::step_addr(uint8_t rn, uint16_t value, uint8_t step, bool dmod)
{
    uint16_t delta = 0;

    bool step2_mode1 = false;
    bool step2_mode2 = false;

    switch (step)
    {
        case 0: //Zero
            return value;
        case 1: //Increment
            delta = 0x1;
            break;
        case 2: //Decrement
            delta = 0xFFFF;
            break;
        case 3: //PlusStep
            if (mod2.br[rn] && !mod2.m[rn])
                delta = (rn < 4) ? stepi0 : stepj0;
            else
            {
                delta = (rn < 4) ? stepi : stepj;
                delta = SignExtend<7>(delta);
            }

            if (mod1.stp16 && !mod1.cmd)
            {
                delta = (rn < 4) ? stepi0 : stepj0;
                if (mod2.m[rn])
                    delta = SignExtend<9>(delta);
            }
            break;
        case 4: //Increase2Mode1
            delta = 0x2;
            step2_mode1 = !mod1.cmd;
            break;
        case 5: //Decrease2Mode1
            delta = 0xFFFE;
            step2_mode1 = !mod1.cmd;
            break;
        default:
            EmuException::die("[DSP] Unrecognized step $%02X in step_addr", step);
    }

    if (!dmod && !mod2.br[rn] && mod2.m[rn])
    {
        uint16_t mod = (rn < 4) ? modi : modj;

        if (!mod || (mod == 1 && step2_mode2))
            return value;

        int iteration = 1;

        if (step2_mode1)
        {
            iteration = 2;
            delta = SignExtend<15, uint16_t>(delta >> 1);
        }

        for (int i = 0; i < iteration; i++)
        {
            if (mod1.cmd || step2_mode2)
            {
                bool neg = false;
                uint16_t m = mod;
                if (delta >> 15)
                {
                    neg = true;
                    m |= ~delta;
                }
                else
                    m |= delta;

                uint16_t mask = (1 << std20_log2p1(m)) - 1;
                uint16_t next;
                if (!neg)
                {
                    if ((value & mask) == mod && (!step2_mode2 || mod != mask))
                        next = 0;
                    else
                        next = (value + delta) & mask;
                }
                else
                {
                    if ((value & mask) == 0 && (!step2_mode2 || mod != mask))
                        next = 0;
                    else
                        next = (value + delta) & mask;
                }

                value &= ~mask;
                value |= next;
            }
            else
            {
                uint16_t mask = (1 << std20_log2p1(mod)) - 1;
                uint16_t next;

                if (delta < 0x8000)
                {
                    next = (value + delta) & mask;
                    if (next == ((mod + 1) & mask))
                        next = 0;
                }
                else
                {
                    next = value & mask;
                    if (next == 0)
                        next = mod + 1;
                    next += delta;
                    next &= mask;
                }

                value &= ~mask;
                value |= next;
            }
        }
    }
    else
        value += delta;

    return value;
}

uint16_t DSP::offset_addr(uint8_t rn, uint16_t addr, uint8_t offset, bool dmod)
{
    if (!offset) //ZeroOffset
        return addr;
    if (offset == 3) //MinusOneDmod
        return addr - 1;

    bool emod = !dmod && !mod2.br[rn] && mod2.m[rn];

    uint16_t mod = (rn < 4) ? modi : modj;
    uint16_t mask = 1;
    for (int i = 0; i < 9; ++i)
        mask |= mod >> i;

    if (offset == 1) //PlusOne
    {
        if (emod && (addr & mask) == mod)
            return addr & ~mask;
        return addr + 1;
    }
    else
    {
        EmuException::die("[DSP] MinusOne in offset_addr");
    }
    return addr;
}

uint16_t DSP::get_ar(int index)
{
    uint16_t value = 0;
    value |= ar[index].step[1];
    value |= ar[index].offset[1] << 3;
    value |= ar[index].step[0] << 5;
    value |= ar[index].offset[0] << 8;
    value |= ar[index].rn[1] << 10;
    value |= ar[index].rn[0] << 13;
    return value;
}

uint16_t DSP::get_arp(int index)
{
    uint16_t value = 0;
    value |= arp[index].step[0];
    value |= arp[index].offset[0] << 3;
    value |= arp[index].step[1] << 5;
    value |= arp[index].offset[1] << 8;
    value |= arp[index].rn[0] << 10;
    value |= arp[index].rn[1] << 13;
    return value;
}

void DSP::set_ar(int index, uint16_t value)
{
    ar[index].step[1] = value & 0x7;
    ar[index].offset[1] = (value >> 3) & 0x3;
    ar[index].step[0] = (value >> 5) & 0x7;
    ar[index].offset[0] = (value >> 8) & 0x3;
    ar[index].rn[1] = (value >> 10) & 0x7;
    ar[index].rn[0] = (value >> 13) & 0x7;
}

void DSP::set_arp(int index, uint16_t value)
{
    arp[index].step[0] = value & 0x7;
    arp[index].offset[0] = (value >> 3) & 0x3;
    arp[index].step[1] = (value >> 5) & 0x7;
    arp[index].offset[1] = (value >> 8) & 0x3;
    arp[index].rn[0] = (value >> 10) & 0x3;
    arp[index].rn[1] = (value >> 13) & 0x3;
}

void DSP::assert_dsp_irq(int id)
{
    printf("[DSP] Assert IRQ: $%02X\n", id);
    icu.int_pending |= 1 << id;
}

void DSP::int_check()
{
    if (mod3.master_int_enable)
    {
        bool irq_found = false;
        for (int id = 0; id < 16; id++)
        {
            if (!(icu.int_pending & (1 << id)))
                continue;
            for (int i = 0; i < 3; i++)
            {
                if (mod3.int_enable[i])
                {
                    if (icu.int_connection[i] & (1 << id))
                    {
                        do_irq(0x0006 + (i * 8), id);
                        irq_found = true;
                        break;
                    }
                }
            }

            if (!irq_found && mod3.vectored_int_enable)
            {
                if (icu.vectored_int_connection & (1 << id))
                    do_irq(icu.vector_addr[id], 3 + id);
            }

            if (irq_found)
                break;
        }
    }
}

void DSP::do_irq(uint32_t addr, uint8_t type)
{
    printf("[DSP] Processing IRQ: $%08X\n", addr);
    mod3.master_int_enable = false;
    unhalt();
    push_pc();
    pc = addr;

    if (type > 2)
    {
        //VINT
        stt2.vectored_int_pending = true;
        if (icu.vector_ctx_switch[type - 3])
            save_context();
    }
    else
    {
        //INT0/INT1/INT2
        stt2.int_pending[type] = true;
        if (mod3.int_ctx_switch[type])
            save_context();
    }
}

void DSP::do_timer_overflow(int index)
{
    assert_dsp_irq(0xA - index);

    switch (timers[index].countup_mode)
    {
        case 0:
            timers[index].enabled = false;
            break;
        case 1:
            timers[index].counter = timers[index].restart_value;
            break;
        case 2:
            break;
        default:
            EmuException::die("[DSP_TIMER%d] Unrecognized countup mode %d in do_timer_overflow",
                              index, timers[index].countup_mode);
    }
}

void DSP::apbp_send_cmd(int index, uint16_t value)
{
    apbp.cmd[index] = value;
    apbp.cmd_ready[index] = true;

    assert_dsp_irq(0xE);
}
