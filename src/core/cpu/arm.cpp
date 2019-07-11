#include <sstream>
#include "arm.hpp"
#include "arm_disasm.hpp"
#include "arm_interpret.hpp"
#include "vfp.hpp"
#include "../common/common.hpp"
#include "../emulator.hpp"

uint64_t ARM_CPU::global_exclusive_start[4];
uint64_t ARM_CPU::global_exclusive_end[4];

uint32_t PSR_Flags::get()
{
    uint32_t reg = 0;
    reg |= negative << 31;
    reg |= zero << 30;
    reg |= carry << 29;
    reg |= overflow << 28;
    reg |= q_overflow << 27;

    reg |= irq_disable << 7;
    reg |= fiq_disable << 6;
    reg |= thumb << 5;

    reg |= mode;
    return reg;
}

void PSR_Flags::set(uint32_t value)
{
    negative = value & (1 << 31);
    zero = value & (1 << 30);
    carry = value & (1 << 29);
    overflow = value & (1 << 28);
    q_overflow = value & (1 << 27);

    irq_disable = value & (1 << 7);
    fiq_disable = value & (1 << 6);
    thumb = value & (1 << 5);

    mode = (PSR_MODE)(value & 0x1F);
}

ARM_CPU::ARM_CPU(Emulator* e, int id, CP15* cp15, VFP* vfp) : e(e), id(id), cp15(cp15), vfp(vfp)
{

}

std::string ARM_CPU::get_reg_name(int id)
{
    if (id < 10)
    {
        std::stringstream output;
        output << "r" << id;
        return output.str();
    }
    else
    {
        switch (id)
        {
            case 10:
                return "sl";
            case 11:
                return "fp";
            case 12:
                return "ip";
            case 13:
                return "sp";
            case 14:
                return "lr";
            case 15:
                return "pc";
        }
    }
    return "??";
}

void ARM_CPU::reset()
{
    CPSR.mode = PSR_SUPERVISOR;
    CPSR.fiq_disable = true;
    CPSR.irq_disable = true;

    tlb_map = cp15->get_tlb_mapping();

    if (id == 9)
        jp(0xFFFF0000, true);
    else
        jp(0, true);

    prefetch_abort_occurred = false;
    can_disassemble = false;
    event_pending = false;
    halted = false;
    waiting_for_event = false;
    int_pending = false;

    local_exclusive_start = 0;
    local_exclusive_end = 0;
}

void ARM_CPU::run(int cycles)
{
    if (halted)
        return;

    try
    {
        int cycles_to_run = cycles;
        while (!halted && cycles_to_run)
        {
            if (CPSR.thumb)
            {
                if (prefetch_abort_occurred)
                    throw EmuException::ARMPrefetchAbort(gpr[15] - 2);
                uint16_t instr = *(uint16_t*)&instr_ptr[(gpr[15] - 2) & 0xFFF];
                if ((gpr[15] & 0xFFF) == 0)
                    fetch_new_instr_ptr(gpr[15]);
                gpr[15] += 2;
                if (can_disassemble)
                {
                    printf("[$%08X] $%04X  %s\n", gpr[15] - 4, instr, ARM_Disasm::disasm_thumb(*this, instr).c_str());
                    //print_state();
                }
                ARM_Interpreter::interpret_thumb(*this, instr);
            }
            else
            {
                if (prefetch_abort_occurred)
                    throw EmuException::ARMPrefetchAbort(gpr[15] - 4);
                uint32_t instr = *(uint32_t*)&instr_ptr[(gpr[15] - 4) & 0xFFF];
                if ((gpr[15] & 0xFFF) == 0)
                    fetch_new_instr_ptr(gpr[15]);
                gpr[15] += 4;
                if (can_disassemble)
                {
                    printf("[$%08X] $%08X  %s\n", gpr[15] - 8, instr, ARM_Disasm::disasm_arm(*this, instr).c_str());
                    //print_state();
                }
                ARM_Interpreter::interpret_arm(*this, instr);
            }
            cycles_to_run--;
        }
    }
    catch (EmuException::ARMDataAbort& a)
    {
        data_abort(a.vaddr, a.is_write);
    }
    catch (EmuException::ARMPrefetchAbort& p)
    {
        prefetch_abort(p.vaddr);
    }

    if (int_pending)
        int_check();
}

void ARM_CPU::print_state()
{
    for (int i = 0; i < 16; i++)
    {
        printf("%s:$%08X", get_reg_name(i).c_str(), gpr[i]);
        if (i % 4 == 3)
            printf("\n");
        else
            printf("\t");
    }
    printf("CPSR:$%08X\n", CPSR.get());
}

void ARM_CPU::jp(uint32_t addr, bool change_thumb_state)
{
    if (addr == 0xC0118F78)
    {
        uint32_t filename = read32(gpr[1]);
        printf("[ARM%d] EXEC PROGRAM: ", id);

        int i = 0;
        while (true)
        {
            uint8_t ch = read8(filename);
            //if (ch == 'i' && i == 1)
                //can_disassemble = true;
            if (!ch)
                break;
            printf("%c", ch);
            filename++;
            i++;
        }
        printf("\n");
    }
    if (addr == 0xC00B8A20)
    {
        uint32_t str_ptr = gpr[0] + 2;
        printf("[ARM%d] PRINTK: ", id);
        while (true)
        {
            uint8_t ch = read8(str_ptr);
            if (!ch)
                break;
            printf("%c", ch);
            str_ptr++;
        }
    }

    if (id != 9 && gpr[15] >= 0x40000000 && ((addr >= 0x00100000 && addr < 0x10000000) || (addr >= 0x14000000 && addr < 0x18000000)))
    {
        //can_disassemble = true;
        uint32_t process_ptr = read32(0xFFFF9004);
        uint32_t pid = read32(process_ptr + 0xB4);
        printf("Jumping to PID%d (addr: $%08X)\n", pid, addr);
        if (addr & 0xFFFFF)
        {
            uint32_t prev_instr = read32(addr - 4);
            if (prev_instr == 0xEF000032)
            {
                uint32_t error = read32(cp15->mrc(0, 0xD, 3, 0) + 0x84);
                if (error & (1 << 31))
                    printf("Error: $%08X\n", error);
            }
        }
        if (pid == 15)
        {
            can_disassemble = true;

        }
        else
            can_disassemble = false;
    }

    gpr[15] = addr;
    fetch_new_instr_ptr(addr);

    if (change_thumb_state)
        CPSR.thumb = addr & 0x1;

    if (CPSR.thumb)
    {
        gpr[15] &= ~0x1;
        gpr[15] += 2;
    }
    else
    {
        gpr[15] &= ~0x3;
        gpr[15] += 4;
    }
}

void ARM_CPU::data_abort(uint32_t addr, bool is_write)
{
    if (id == 9)
        EmuException::die("[ARM%d] Data abort at vaddr $%08X", id, addr);

    printf("[ARM%d] Data abort at vaddr $%08X\n", id, addr);

    cp15->set_data_abort_regs(addr, is_write);

    uint32_t value = CPSR.get();
    SPSR[PSR_ABORT].set(value);
    LR_abt = gpr[15];

    update_reg_mode(PSR_ABORT);
    CPSR.mode = PSR_ABORT;
    CPSR.irq_disable = true;

    if (cp15->has_high_exceptions())
        jp(0xFFFF0000 + 0x10, true);
    else
        jp(0x10, true);
}

void ARM_CPU::prefetch_abort(uint32_t addr)
{
    prefetch_abort_occurred = false;
    if (id == 9)
        EmuException::die("[ARM%d] Prefetch abort at vaddr $%08X\n", id, addr);

    printf("[ARM%d] Prefetch abort at vaddr $%08X\n", id, addr);

    cp15->set_prefetch_abort_regs(addr);

    uint32_t value = CPSR.get();
    SPSR[PSR_ABORT].set(value);
    LR_abt = addr;
    LR_abt += (CPSR.thumb) ? 2 : 4;

    update_reg_mode(PSR_ABORT);
    CPSR.mode = PSR_ABORT;
    CPSR.irq_disable = true;

    if (cp15->has_high_exceptions())
        jp(0xFFFF0000 + 0x0C, true);
    else
        jp(0x0C, true);
}

void ARM_CPU::swi()
{
    uint8_t op;
    if (!CPSR.thumb)
        op = read32(gpr[15] - 8) & 0xFF;
    else
        op = read32(gpr[15] - 4) & 0xFF;

    if (id == 9)
    {
        //printf("SWI $%02X!\n", op);
        if (op == 0xFF)
        {
            unsigned char c = gpr[0] & 0xFF;
            if (c == 0xFF)
                printf("\n");
            else
                printf("%c", c);
            return;
        }
    }
    else
    {
        //uint32_t process_ptr = read32(0xFFFF9004);
        //printf("SVC $%02X, PID%d\n", op, read32(process_ptr + 0xB4));
        printf("SVC $%02X!\n", op);
        //printf("SVC $%04X!\n", gpr[7]);
        if (op == 0x32)
        {
            uint32_t tls = cp15->mrc(0, 0xD, 0x3, 0x0);
            uint32_t header = read32(tls + 0x80);
            uint32_t process_ptr = read32(0xFFFF9004);
            uint32_t pid = read32(process_ptr + 0xB4);
            printf("(PID%d) SendSyncRequest: $%08X\n", pid, header);
        }
    }
    /*if (gpr[7] == 4)
    {
        //can_disassemble = false;
        uint32_t buf = gpr[1];
        uint32_t count = gpr[2];

        printf("SYS_WRITE: ");
        while (count)
        {
            printf("%c", read8(buf));
            buf++;
            count--;
        }
    }*/
    if (op == 0x3C)
    {
        EmuException::die("[ARM%d] svcBreak called!", id);
    }
    //print_state();
    //can_disassemble = true;

    uint32_t value = CPSR.get();
    SPSR[PSR_SUPERVISOR].set(value);

    LR_svc = gpr[15];
    LR_svc -= (!CPSR.thumb) ? 4 : 2;

    update_reg_mode(PSR_SUPERVISOR);
    CPSR.mode = PSR_SUPERVISOR;
    CPSR.irq_disable = true;

    if (cp15->has_high_exceptions())
        jp(0xFFFF0000 + 0x08, true);
    else
        jp(0x08, true);
}

void ARM_CPU::und()
{
    uint32_t value = CPSR.get();
    SPSR[PSR_UNDEFINED].set(value);

    LR_und = gpr[15] - 4;
    update_reg_mode(PSR_UNDEFINED);
    CPSR.mode = PSR_UNDEFINED;
    CPSR.irq_disable = true;

    if (cp15->has_high_exceptions())
        jp(0xFFFF0000 + 0x04, true);
    else
        jp(0x04, true);
}

void ARM_CPU::int_check()
{
    //printf("[ARM%d] Interrupt check\n", id);
    if (!CPSR.irq_disable)
    {
        printf("[ARM%d] Interrupt!\n", id);
        uint32_t value = CPSR.get();
        SPSR[PSR_IRQ].set(value);

        //Update new CPSR
        LR_irq = gpr[15] + ((CPSR.thumb) ? 2 : 0);
        update_reg_mode(PSR_IRQ);
        CPSR.mode = PSR_IRQ;
        CPSR.irq_disable = true;

        if (cp15->has_high_exceptions())
            jp(0xFFFF0000 + 0x18, true);
        else
            jp(0x18, true);
    }
}

void ARM_CPU::set_int_signal(bool pending)
{
    if (!int_pending && pending)
    {
        unhalt();
        int_check();
    }
    int_pending = pending;
    //printf("[ARM%d] Set int signal: %d\n", id, pending);
}

void ARM_CPU::halt()
{
    if (!int_pending)
    {
        //printf("[ARM%d] Halting... $%08X\n", id, CPSR.get());
        halted = true;
    }
}

void ARM_CPU::unhalt()
{
    halted = false;
}

void ARM_CPU::wfe()
{
    if (!event_pending)
    {
        printf("[ARM%d] WFE\n", id);
        waiting_for_event = true;
        halted = true;
    }
    else
        event_pending = false;
}

void ARM_CPU::sev()
{
    e->arm11_send_events(id);
}

void ARM_CPU::send_event(int id)
{
    if (this->id == id)
        return;
    if (waiting_for_event)
    {
        printf("[ARM%d] WFE cancelled\n", id);
        halted = false;
        waiting_for_event = false;
    }
    else
        event_pending = true;
}

void ARM_CPU::set_zero_neg_flags(uint32_t value)
{
    CPSR.negative = value & (1 << 31);
    CPSR.zero = !value;
}

//Code here from melonDS's ALU core (my original implementation was incorrect in many ways)
void ARM_CPU::set_CV_add_flags(uint32_t a, uint32_t b, uint32_t result)
{
    CPSR.carry = ((0xFFFFFFFF-a) < b);
    CPSR.overflow = ADD_OVERFLOW(a, b, result);
}

void ARM_CPU::set_CV_sub_flags(uint32_t a, uint32_t b, uint32_t result)
{
    CPSR.carry = a >= b;
    CPSR.overflow = SUB_OVERFLOW(a, b, result);
}

void ARM_CPU::update_reg_mode(PSR_MODE mode)
{
    if (mode != CPSR.mode)
    {
        switch (CPSR.mode)
        {
            case PSR_SYSTEM:
            case PSR_USER:
                break;
            case PSR_IRQ:
                std::swap(gpr[13], SP_irq);
                std::swap(gpr[14], LR_irq);
                break;
            case PSR_FIQ:
                std::swap(gpr[8], fiq_regs[0]);
                std::swap(gpr[9], fiq_regs[1]);
                std::swap(gpr[10], fiq_regs[2]);
                std::swap(gpr[11], fiq_regs[3]);
                std::swap(gpr[12], fiq_regs[4]);
                std::swap(gpr[13], SP_fiq);
                std::swap(gpr[14], LR_fiq);
                break;
            case PSR_SUPERVISOR:
                std::swap(gpr[13], SP_svc);
                std::swap(gpr[14], LR_svc);
                break;
            case PSR_ABORT:
                std::swap(gpr[13], SP_abt);
                std::swap(gpr[14], LR_abt);
                break;
            case PSR_UNDEFINED:
                std::swap(gpr[13], SP_und);
                std::swap(gpr[14], LR_und);
                break;
            default:
                EmuException::die("[ARM%d] Unrecognized old PSR mode %d\n", id, CPSR.mode);
        }

        switch (mode)
        {
            case PSR_SYSTEM:
            case PSR_USER:
                break;
            case PSR_IRQ:
                std::swap(gpr[13], SP_irq);
                std::swap(gpr[14], LR_irq);
                break;
            case PSR_FIQ:
                std::swap(gpr[8], fiq_regs[0]);
                std::swap(gpr[9], fiq_regs[1]);
                std::swap(gpr[10], fiq_regs[2]);
                std::swap(gpr[11], fiq_regs[3]);
                std::swap(gpr[12], fiq_regs[4]);
                std::swap(gpr[13], SP_fiq);
                std::swap(gpr[14], LR_fiq);
                break;
            case PSR_SUPERVISOR:
                std::swap(gpr[13], SP_svc);
                std::swap(gpr[14], LR_svc);
                break;
            case PSR_ABORT:
                std::swap(gpr[13], SP_abt);
                std::swap(gpr[14], LR_abt);
                break;
            case PSR_UNDEFINED:
                std::swap(gpr[13], SP_und);
                std::swap(gpr[14], LR_und);
                break;
            default:
                EmuException::die("[ARM%d] Unrecognized new PSR mode %d\n", id, mode);
        }
    }
}

void ARM_CPU::spsr_to_cpsr()
{
    uint32_t new_CPSR = SPSR[CPSR.mode].get();
    update_reg_mode((PSR_MODE)(new_CPSR & 0x1F));
    CPSR.set(new_CPSR);
}

bool ARM_CPU::meets_condition(int cond)
{
    switch (cond)
    {
        case 0x0:
            //EQ - equal
            return CPSR.zero;
        case 0x1:
            //NE - not equal
            return !CPSR.zero;
        case 0x2:
            //CS - unsigned higher or same
            return CPSR.carry;
        case 0x3:
            //CC - unsigned lower
            return !CPSR.carry;
        case 0x4:
            //MI - negative
            return CPSR.negative;
        case 0x5:
            //PL - positive or zero
            return !CPSR.negative;
        case 0x6:
            //VS - overflow
            return CPSR.overflow;
        case 0x7:
            //VC - no overflow
            return !CPSR.overflow;
        case 0x8:
            //HI - unsigned higher
            return CPSR.carry && !CPSR.zero;
        case 0x9:
            //LS - unsigned lower or same
            return !CPSR.carry || CPSR.zero;
        case 0xA:
            //GE - greater than or equal
            return (CPSR.negative == CPSR.overflow);
        case 0xB:
            //LT - less than
            return (CPSR.negative != CPSR.overflow);
        case 0xC:
            //GT - greater than
            return !CPSR.zero && (CPSR.negative == CPSR.overflow);
        case 0xD:
            //LE - less than or equal to
            return CPSR.zero || (CPSR.negative != CPSR.overflow);
        case 0xE:
            //AL - always
            return true;
        case 0xF:
            //Some instructions have the 0xF condition required, so let them pass
            return true;
        default:
            EmuException::die("[ARM_CPU] Unrecognized condition %d\n", cond);
            return false;
    }
}

void ARM_CPU::fetch_new_instr_ptr(uint32_t addr)
{
    uint64_t mem = (uint64_t)tlb_map[addr / 4096];
    if (!(mem & (1UL << 60UL)))
    {
        cp15->reload_tlb(addr);
        mem = (uint64_t)tlb_map[addr / 4096];
        if (!(mem & (1UL << 60UL)))
            prefetch_abort_occurred = true;
    }
    if (mem & (1UL << 63UL))
        EmuException::die("[ARM%d] PC points to MMIO $%08X", id, addr);
    mem &= ~(0xFUL << 60UL);
    instr_ptr = (uint8_t*)mem;
}

uint8_t ARM_CPU::read8(uint32_t addr)
{
    uint64_t mem = (uint64_t)tlb_map[addr / 4096];
    if (!(mem & (1UL << 62UL)))
    {
        //TLB miss - reload and check the vaddr again
        cp15->reload_tlb(addr);
        mem = (uint64_t)tlb_map[addr / 4096];
        if (!(mem & (1UL << 62UL)))
            throw EmuException::ARMDataAbort(addr, false);
    }
    if (!(mem & (1UL << 63UL)))
    {
        mem &= ~(0xFUL << 60UL);
        uint8_t* ptr = (uint8_t*)mem;
        return ptr[addr & 0xFFF];
    }
    else
        addr = (mem & 0xFFFFF000) + (addr & 0xFFF);
    if (id == 9)
        return e->arm9_read8(addr);
    return e->arm11_read8(id - 11, addr);
}

uint16_t ARM_CPU::read16(uint32_t addr)
{
    if ((addr & 0xFFF) > 0xFFE)
        EmuException::die("[ARM%d] Unaligned read16 on page boundary $%08X", id, addr);
    if (id == 9 && (addr & 0x1))
        EmuException::die("[ARM9] Unaligned read16 $%08X", addr);
    uint64_t mem = (uint64_t)tlb_map[addr / 4096];
    if (!(mem & (1UL << 62UL)))
    {
        //TLB miss - reload and check the vaddr again
        cp15->reload_tlb(addr);
        mem = (uint64_t)tlb_map[addr / 4096];
        if (!(mem & (1UL << 62UL)))
            throw EmuException::ARMDataAbort(addr, false);
    }
    if (!(mem & (1UL << 63UL)))
    {
        mem &= ~(0xFUL << 60UL);
        uint8_t* ptr = (uint8_t*)mem;
        return *(uint16_t*)&ptr[addr & 0xFFF];
    }
    else
        addr = (mem & 0xFFFFF000) + (addr & 0xFFF);
    if (id == 9)
        return e->arm9_read16(addr);
    return e->arm11_read16(id - 11, addr);
}

uint32_t ARM_CPU::read32(uint32_t addr)
{
    if (id == 9 && (addr & 0x3))
        EmuException::die("[ARM9] Unaligned read32 $%08X", addr);
    if ((addr & 0xFFF) > 0xFFC)
        EmuException::die("[ARM%d] Unaligned read32 on page boundary $%08X", id, addr);
    uint64_t mem = (uint64_t)tlb_map[addr / 4096];
    if (!(mem & (1UL << 62UL)))
    {
        //TLB miss - reload and check the vaddr again
        cp15->reload_tlb(addr);
        mem = (uint64_t)tlb_map[addr / 4096];
        if (!(mem & (1UL << 62UL)))
            throw EmuException::ARMDataAbort(addr, false);
    }
    if (!(mem & (1UL << 63UL)))
    {
        mem &= ~(0xFUL << 60UL);
        uint8_t* ptr = (uint8_t*)mem;
        return *(uint32_t*)&ptr[addr & 0xFFF];
    }
    else
        addr = (mem & 0xFFFFF000) + (addr & 0xFFF);
    if (id == 9)
        return e->arm9_read32(addr);
    return e->arm11_read32(id - 11, addr);
}

uint64_t ARM_CPU::read64(uint32_t addr)
{
    uint64_t value = read32(addr + 4);
    value <<= 32ULL;
    return value | read32(addr);
}

void ARM_CPU::write8(uint32_t addr, uint8_t value)
{
    if (addr >= 0x080B7BC0 && addr < 0x080B7BC0 + 0x200)
    {
        printf("[ARM9] Write8 blorp $%08X: $%02X\n", addr, value);
    }
    uint64_t mem = (uint64_t)tlb_map[addr / 4096];
    if (!(mem & (1UL << 61UL)))
    {
        //TLB miss - reload and check the vaddr again
        cp15->reload_tlb(addr);
        mem = (uint64_t)tlb_map[addr / 4096];
        if (!(mem & (1UL << 62UL)))
            throw EmuException::ARMDataAbort(addr, true);
    }
    if (!(mem & (1UL << 63UL)))
    {
        mem &= ~(0xFUL << 60UL);
        uint8_t* ptr = (uint8_t*)mem;
        ptr[addr & 0xFFF] = value;
        return;
    }
    else
        addr = (mem & 0xFFFFF000) + (addr & 0xFFF);
    if (id == 9)
        e->arm9_write8(addr, value);
    else
        e->arm11_write8(id - 11, addr, value);
}

void ARM_CPU::write16(uint32_t addr, uint16_t value)
{
    if (id == 9 && (addr & 0x1))
        EmuException::die("[ARM9] Unaligned write16 $%08X: $%04X", addr, value);
    if ((addr & 0xFFF) > 0xFFE)
        EmuException::die("[ARM%d] Unaligned write16 on page boundary $%08X: $%08X", id, addr, value);
    uint64_t mem = (uint64_t)tlb_map[addr / 4096];
    if (!(mem & (1UL << 61UL)))
    {
        //TLB miss - reload and check the vaddr again
        cp15->reload_tlb(addr);
        mem = (uint64_t)tlb_map[addr / 4096];
        if (!(mem & (1UL << 62UL)))
            throw EmuException::ARMDataAbort(addr, true);
    }
    if (!(mem & (1UL << 63UL)))
    {
        mem &= ~(0xFUL << 60UL);
        uint8_t* ptr = (uint8_t*)mem;
        *(uint16_t*)&ptr[addr & 0xFFF] = value;
        return;
    }
    else
        addr = (mem & 0xFFFFF000) + (addr & 0xFFF);
    if (id == 9)
        e->arm9_write16(addr, value);
    else
        e->arm11_write16(id - 11, addr, value);
}

void ARM_CPU::write32(uint32_t addr, uint32_t value)
{
    if (id == 9 && (addr & 0x3))
        EmuException::die("[ARM9] Unaligned write32 $%08X: $%08X", addr, value);
    if ((addr & 0xFFF) > 0xFFC)
        EmuException::die("[ARM%d] Unaligned write32 on page boundary $%08X: $%08X", id, addr, value);
    uint64_t mem = (uint64_t)tlb_map[addr / 4096];
    if (!(mem & (1UL << 61UL)))
    {
        //TLB miss - reload and check the vaddr again
        if (id == 9 && (addr & 0xF0000000) == 0xC0000000)
            return;
        cp15->reload_tlb(addr);
        mem = (uint64_t)tlb_map[addr / 4096];
        if (!(mem & (1UL << 61UL)))
            throw EmuException::ARMDataAbort(addr, true);
    }
    if (!(mem & (1UL << 63UL)))
    {
        mem &= ~(0xFUL << 60UL);
        uint8_t* ptr = (uint8_t*)mem;
        *(uint32_t*)&ptr[addr & 0xFFF] = value;
        return;
    }
    else
        addr = (mem & 0xFFFFF000) + (addr & 0xFFF);
    if (id == 9)
        e->arm9_write32(addr, value);
    else
        e->arm11_write32(id - 11, addr, value);
}

void ARM_CPU::write64(uint32_t addr, uint64_t value)
{
    write32(addr, value & 0xFFFFFFFF);
    write32(addr + 4, value >> 32);
}

bool ARM_CPU::has_exclusive(uint32_t addr)
{
    uint64_t paddr = (uint64_t)tlb_map[addr / 4096] + (addr & 0xFFF);
    paddr &= ~(0xFFUL << 56UL);

    if (paddr < local_exclusive_start || paddr > local_exclusive_end)
        return false;

    int core = id - 11;

    if (paddr < global_exclusive_start[core] || paddr > global_exclusive_end[core])
        return false;

    return true;
}

void ARM_CPU::set_exclusive(uint32_t addr, uint32_t size)
{
    uint64_t paddr = (uint64_t)tlb_map[addr / 4096] + (addr & 0xFFF);

    paddr &= ~(0xFFUL << 56UL);
    local_exclusive_start = paddr;
    local_exclusive_end = paddr + size;

    global_exclusive_start[id - 11] = local_exclusive_start;
    global_exclusive_end[id - 11] = local_exclusive_end;
}

void ARM_CPU::clear_global_exclusives(uint32_t addr)
{
    if (id < 11)
        return;
    uint64_t paddr = (uint64_t)tlb_map[addr / 4096] + (addr & 0xFFF);

    paddr &= ~(0xFFUL << 56UL);

    for (int i = 0; i < 4; i++)
    {
        if (global_exclusive_start[i] <= paddr && paddr < global_exclusive_end[i])
        {
            global_exclusive_start[i] = 0;
            global_exclusive_end[i] = 0;
        }
    }
}

void ARM_CPU::clear_exclusive()
{
    local_exclusive_start = 0;
    local_exclusive_end = 0;
    global_exclusive_start[id - 11] = 0;
    global_exclusive_end[id - 11] = 0;
}

void ARM_CPU::andd(int destination, int source, int operand, bool set_condition_codes)
{
    uint32_t result = source & operand;
    set_register(destination, result);

    if (set_condition_codes)
        set_zero_neg_flags(result);
}

void ARM_CPU::orr(int destination, int source, int operand, bool set_condition_codes)
{
    uint32_t result = source | operand;
    set_register(destination, result);

    if (set_condition_codes)
        set_zero_neg_flags(result);
}

void ARM_CPU::eor(int destination, int source, int operand, bool set_condition_codes)
{
    uint32_t result = source ^ operand;
    set_register(destination, result);

    if (set_condition_codes)
        set_zero_neg_flags(result);
}

void ARM_CPU::add(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint64_t unsigned_result = source + operand;
    if (destination == REG_PC)
    {
        if (set_condition_codes)
            EmuException::die("adds pc, operand unsupported");
        else
            jp(unsigned_result & 0xFFFFFFFF, true);
    }
    else
    {
        set_register(destination, unsigned_result & 0xFFFFFFFF);

        if (set_condition_codes)
            cmn(source, operand);
    }
}

void ARM_CPU::sub(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint64_t unsigned_result = source - operand;
    if (destination == REG_PC)
    {
        if (set_condition_codes)
        {
            int index = static_cast<int>(CPSR.mode);
            update_reg_mode(SPSR[index].mode);
            CPSR.set(SPSR[index].get());
            jp(unsigned_result & 0xFFFFFFFF, false);
        }
        else
            jp(unsigned_result & 0xFFFFFFFF, true);
    }
    else
    {
        set_register(destination, unsigned_result & 0xFFFFFFFF);
        if (set_condition_codes)
            cmp(source, operand);
    }
}

void ARM_CPU::adc(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint8_t carry = (CPSR.carry) ? 1 : 0;
    add(destination, source + carry, operand, set_condition_codes);
    if (set_condition_codes)
    {
        uint32_t temp = source + operand;
        uint32_t res = temp + carry;
        CPSR.carry = CARRY_ADD(source, operand) | CARRY_ADD(temp, carry);
        CPSR.overflow = ADD_OVERFLOW(source, operand, temp) | ADD_OVERFLOW(temp, carry, res);
    }
}

void ARM_CPU::sbc(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    unsigned int borrow = (CPSR.carry) ? 0 : 1;
    sub(destination, source, operand + borrow, set_condition_codes);
    if (set_condition_codes)
    {
        uint32_t temp = source - operand;
        uint32_t res = temp - borrow;
        CPSR.carry = CARRY_SUB(source, operand) & CARRY_SUB(temp, borrow);
        CPSR.overflow = SUB_OVERFLOW(source, operand, temp) | SUB_OVERFLOW(temp, borrow, res);
    }
}

void ARM_CPU::tst(uint32_t x, uint32_t y)
{
    set_zero_neg_flags(x & y);
}

void ARM_CPU::teq(uint32_t x, uint32_t y)
{
    set_zero_neg_flags(x ^ y);
}

void ARM_CPU::cmn(uint32_t x, uint32_t y)
{
    uint32_t result = x + y;
    set_zero_neg_flags(result);
    set_CV_add_flags(x, y, result);
}

void ARM_CPU::cmp(uint32_t x, uint32_t y)
{
    uint32_t result = x - y;
    set_zero_neg_flags(result);
    set_CV_sub_flags(x, y, result);
}

void ARM_CPU::mov(uint32_t destination, uint32_t operand, bool alter_flags)
{
    if (destination == REG_PC)
    {
        if (alter_flags)
        {
            int index = static_cast<int>(CPSR.mode);
            update_reg_mode(SPSR[index].mode);
            CPSR.set(SPSR[index].get());
            jp(operand, false);
        }
        else
            jp(operand, true);
    }
    else
    {
        set_register(destination, operand);
        if (alter_flags)
            set_zero_neg_flags(operand);
    }
}

void ARM_CPU::mul(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes)
{
    uint64_t result = source * operand;
    uint32_t truncated = result & 0xFFFFFFFF;
    set_register(destination, truncated);

    if (set_condition_codes)
        set_zero_neg_flags(truncated);
}

void ARM_CPU::bic(uint32_t destination, uint32_t source, uint32_t operand, bool alter_flags)
{
    uint32_t result = source & ~operand;
    set_register(destination, result);
    if (alter_flags)
        set_zero_neg_flags(result);
}

void ARM_CPU::mvn(uint32_t destination, uint32_t operand, bool alter_flags)
{
    set_register(destination, ~operand);
    if (alter_flags)
        set_zero_neg_flags(~operand);
}

void ARM_CPU::mrs(uint32_t instr)
{
    bool using_CPSR = (instr & (1 << 22)) == 0;
    uint32_t destination = (instr >> 12) & 0xF;

    if (using_CPSR)
    {
        set_register(destination, CPSR.get());
    }
    else
    {
        set_register(destination, SPSR[static_cast<int>(CPSR.mode)].get());
    }
}

void ARM_CPU::msr(uint32_t instr)
{
    bool is_imm = (instr & (1 << 25));
    bool using_CPSR = (instr & (1 << 22)) == 0;

    PSR_Flags* PSR;
    if (using_CPSR)
        PSR = &CPSR;
    else
        PSR = &SPSR[static_cast<int>(CPSR.mode)];

    uint32_t value = PSR->get();
    uint32_t source;
    if (is_imm)
    {
        source = instr & 0xFF;
        int shift = (instr & 0xF00) >> 7;
        source = rotr32(source, shift, false);
    }
    else
        source = get_register(instr & 0xF);

    uint32_t bitmask = 0;
    if (instr & (1 << 19))
        bitmask |= 0xFF000000;

    if (instr & (1 << 16))
        bitmask |= 0xFF;

    if (CPSR.mode == PSR_USER)
        bitmask &= 0xFFFFFF00; //prevent changes to control field

    if (using_CPSR)
        bitmask &= 0xFFFFFFDF; //prevent changes to thumb state

    value &= ~bitmask;
    value |= (source & bitmask);

    if (using_CPSR)
    {
        PSR_MODE new_mode = static_cast<PSR_MODE>(value & 0x1F);
        update_reg_mode(new_mode);
    }
    PSR->set(value);
}

void ARM_CPU::cps(uint32_t instr)
{
    if (CPSR.mode == PSR_USER)
        return;
    PSR_MODE psr_mode = (PSR_MODE)(instr & 0x1F);
    bool f = (instr >> 6) & 0x1;
    bool i = (instr >> 7) & 0x1;
    bool a = (instr >> 8) & 0x1;
    bool mmod = (instr >> 17) & 0x1;
    int imod = (instr >> 18) & 0x3;

    if (mmod)
    {
        update_reg_mode(psr_mode);
        CPSR.mode = psr_mode;
    }

    if (imod == 2)
    {
        //TODO: data abort
        CPSR.fiq_disable &= ~f;
        CPSR.irq_disable &= ~i;
    }
    else if (imod == 3)
    {
        CPSR.fiq_disable |= f;
        CPSR.irq_disable |= i;
    }
}

void ARM_CPU::srs(uint32_t instr)
{
    bool is_writing_back = instr & (1 << 21);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);

    PSR_MODE mode = (PSR_MODE)(instr & 0x1F);

    uint32_t saved_LR = gpr[REG_LR];
    uint32_t saved_PSR = SPSR[CPSR.mode].get();

    PSR_MODE old_mode = CPSR.mode;
    update_reg_mode(mode);
    CPSR.mode = mode;

    uint32_t banked_addr = get_register(REG_SP);

    int offset;
    if (is_adding_offset)
    {
        offset = 4;

        if (is_preindexing)
        {
            write32(banked_addr + offset, saved_LR);
            write32(banked_addr + (offset * 2), saved_PSR);
        }
        else
        {
            write32(banked_addr, saved_LR);
            write32(banked_addr + offset, saved_PSR);
        }
    }
    else
    {
        offset = -4;

        if (is_preindexing)
        {
            write32(banked_addr + offset, saved_PSR);
            write32(banked_addr + (offset * 2), saved_LR);
        }
        else
        {
            write32(banked_addr, saved_PSR);
            write32(banked_addr + offset, saved_LR);
        }
    }

    if (is_writing_back)
        set_register(REG_SP, banked_addr + (offset * 2));

    //Restore previous state
    update_reg_mode(old_mode);
    CPSR.mode = old_mode;
}

void ARM_CPU::rfe(uint32_t instr)
{
    bool is_writing_back = instr & (1 << 21);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);

    int base = (instr >> 16) & 0xF;

    uint32_t addr = get_register(base);
    uint32_t PC, PSR;
    int offset;
    if (is_adding_offset)
    {
        offset = 4;

        if (is_preindexing)
        {
            PC = read32(addr + offset);
            PSR = read32(addr + (offset * 2));
        }
        else
        {
            PC = read32(addr);
            PSR = read32(addr + offset);
        }
    }
    else
    {
        offset = -4;

        if (is_preindexing)
        {
            PSR = read32(addr + offset);
            PC = read32(addr + (offset * 2));
        }
        else
        {
            PSR = read32(addr);
            PC = read32(addr + offset);
        }
    }

    if (is_writing_back)
        set_register(base, addr + (offset * 2));

    update_reg_mode((PSR_MODE)(PSR & 0x1F));
    CPSR.set(PSR);

    jp(PC, false);
}

void ARM_CPU::vfp_single_transfer(uint32_t instr)
{
    ARM_Interpreter::vfp_single_transfer(*this, *vfp, instr);
}

void ARM_CPU::vfp_load_store(uint32_t instr)
{
    ARM_Interpreter::vfp_load_store(*this, *vfp, instr);
}

void ARM_CPU::vfp_data_processing(uint32_t instr)
{
    ARM_Interpreter::vfp_data_processing(*this, *vfp, instr);
}

uint32_t ARM_CPU::mrc(int coprocessor_id, int operation_mode, int CP_reg,
                  int coprocessor_info, int coprocessor_operand)
{
    switch (coprocessor_id)
    {
        case 15:
            if (!cp15)
                return 0;
            return cp15->mrc(operation_mode, CP_reg, coprocessor_info, coprocessor_operand);
        default:
            return 0;
    }
}

void ARM_CPU::mcr(int coprocessor_id, int operation_mode,
                  int CP_reg, int coprocessor_info, int coprocessor_operand, uint32_t value)
{
    switch (coprocessor_id)
    {
        case 15:
            cp15->mcr(operation_mode, CP_reg, coprocessor_info, coprocessor_operand, value);

            tlb_map = cp15->get_tlb_mapping();
            break;
    }
}

uint32_t ARM_CPU::lsl(uint32_t value, int shift, bool alter_flags)
{
    if (!shift)
    {
        if (alter_flags)
            set_zero_neg_flags(value);
        return value;
    }
    if (shift > 32)
    {
        if (alter_flags)
        {
            set_zero_neg_flags(0);
            CPSR.carry = false;
        }
        return 0;
    }
    if (shift > 31)
    {
        if (alter_flags)
        {
            set_zero_neg_flags(0);
            CPSR.carry = value & (1 << 0);
        }
        return 0;
    }
    uint32_t result = value << shift;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        CPSR.carry = value & (1 << (32 - shift));
    }
    return value << shift;
}

uint32_t ARM_CPU::lsr(uint32_t value, int shift, bool alter_flags)
{
    if (shift > 32)
    {
        if (alter_flags)
        {
            set_zero_neg_flags(0);
            CPSR.carry = false;
        }
        return 0;
    }
    if (shift > 31)
        return lsr_32(value, alter_flags);
    uint32_t result = value >> shift;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        if (shift)
            CPSR.carry = value & (1 << (shift - 1));
    }
    return result;
}

uint32_t ARM_CPU::lsr_32(uint32_t value, bool alter_flags)
{
    if (alter_flags)
    {
        set_zero_neg_flags(0);
        CPSR.carry = value & (1 << 31);
    }
    return 0;
}

uint32_t ARM_CPU::asr(uint32_t value, int shift, bool alter_flags)
{
    if (shift > 31)
        return asr_32(value, alter_flags);
    uint32_t result = static_cast<int32_t>(value) >> shift;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        if (shift)
            CPSR.carry = value & (1 << (shift - 1));
    }
    return result;
}

uint32_t ARM_CPU::asr_32(uint32_t value, bool alter_flags)
{
    uint32_t result = static_cast<int32_t>(value) >> 31;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        CPSR.carry = value & (1 << 31);
    }
    return result;
}

uint32_t ARM_CPU::rrx(uint32_t value, bool alter_flags)
{
    uint32_t result = value;
    result >>= 1;
    result |= (CPSR.carry) ? (1 << 31) : 0;
    if (alter_flags)
    {
        set_zero_neg_flags(result);
        CPSR.carry = value & 0x1;
    }
    return result;
}

//Thanks to Stack Overflow for providing a safe rotate function
uint32_t ARM_CPU::rotr32(uint32_t n, unsigned int c, bool alter_flags)
{
    const unsigned int mask = 0x1F;
    if (alter_flags && c)
    {
        if (c & 0x1F)
            CPSR.carry = n & (1 << (c - 1));
        else
            CPSR.carry = n & (1 << 31);
    }
    c &= mask;

    uint32_t result = (n>>c) | (n<<( (-c)&mask ));

    if (alter_flags)
    {
        set_zero_neg_flags(result);
    }

    return result;
}
