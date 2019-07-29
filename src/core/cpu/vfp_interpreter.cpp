#include <cmath>
#include "../common/common.hpp"
#include "arm.hpp"
#include "arm_interpret.hpp"
#include "vfp.hpp"

namespace ARM_Interpreter
{

void vfp_single_transfer(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    uint8_t op = (instr >> 21) & 0x7;
    bool op_hi = (instr >> 8) & 0x1;

    op |= op_hi << 4;

    switch (op)
    {
        case 0x00:
            if (!vfp.is_enabled())
            {
                cpu.und();
                return;
            }
            vfp_mov_fpr_gpr(cpu, vfp, instr);
            break;
        case 0x07:
            vfp_mov_sys_reg(cpu, vfp, instr);
            break;
        default:
            EmuException::die("[VFP_Interpreter] Unrecognized single transfer op $%02X ($%08X)", op, instr);
    }
}

void vfp_mov_fpr_gpr(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    bool load_to_gpr = (instr >> 20) & 0x1;
    int gpr = (instr >> 12) & 0xF;
    int fpr = (instr >> 16) & 0xF;
    bool fpr_hi = (instr >> 7) & 0x1;

    fpr <<= 1;
    fpr |= fpr_hi;

    if (load_to_gpr)
        cpu.set_register(gpr, vfp.get_reg32(fpr));
    else
        vfp.set_reg32(fpr, cpu.get_register(gpr));
}

void vfp_mov_sys_reg(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    bool load_to_gpr = (instr >> 20) & 0x1;
    int gpr = (instr >> 12) & 0xF;
    int sys = (instr >> 16) & 0xF;
    bool sys_hi = (instr >> 7) & 0x1;

    if (sys_hi)
        return;

    switch (sys)
    {
        case 0x0:
            if (load_to_gpr)
                cpu.set_register(gpr, 0x410120B4);
            break;
        case 0x1:
            if (!vfp.is_enabled())
            {
                cpu.und();
                return;
            }
            if (load_to_gpr)
            {
                uint32_t fpscr = vfp.get_fpscr();
                if (gpr == 15)
                {
                    //FMSTAT - Modify CPSR flags
                    PSR_Flags* cpsr = cpu.get_CPSR();

                    cpsr->overflow = (fpscr >> 28) & 0x1;
                    cpsr->carry = (fpscr >> 29) & 0x1;
                    cpsr->zero = (fpscr >> 30) & 0x1;
                    cpsr->negative = (fpscr >> 31) & 0x1;
                }
                else
                {
                    cpu.set_register(gpr, fpscr);
                }
            }
            else
            {
                uint32_t value = cpu.get_register(gpr);
                vfp.set_fpscr(value);
            }
            break;
        case 0x8:
            if (load_to_gpr)
            {
                uint32_t fpexc = vfp.get_fpexc();
                cpu.set_register(gpr, fpexc);
            }
            else
            {
                uint32_t value = cpu.get_register(gpr);
                vfp.set_fpexc(value);
            }
            break;
    }
}

void vfp_load_store(ARM_CPU& cpu, VFP &vfp, uint32_t instr)
{
    if (!vfp.is_enabled())
    {
        cpu.und();
        return;
    }

    bool p = (instr >> 24) & 0x1;
    bool u = (instr >> 23) & 0x1;
    bool w = (instr >> 21) & 0x1;
    bool l = (instr >> 20) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    uint16_t op = (is_double << 4) | (l << 3) | (p << 2) | (u << 1) | w;

    switch (op)
    {
        case 0x00:
        case 0x08:
        case 0x10:
        case 0x18:
            vfp_load_store_two(cpu, vfp, instr);
            break;
        case 0x0A:
        case 0x1A:
        case 0x0B:
        case 0x1B:
            vfp_load_block(cpu, vfp, instr);
            break;
        case 0x02:
        case 0x12:
        case 0x03:
        case 0x13:
        case 0x05:
        case 0x15:
            vfp_store_block(cpu, vfp, instr);
            break;
        case 0x06:
        case 0x16:
            vfp_store_single(cpu, vfp, instr);
            break;
        case 0x0C:
        case 0x0E:
        case 0x1C:
        case 0x1E:
            vfp_load_single(cpu, vfp, instr);
            break;
        default:
            EmuException::die("[VFP_Interpreter] Undefined load/store instr $%04X ($%08X)\n", op, instr);
    }
}

void vfp_load_store_two(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    uint32_t reg2 = (instr >> 16) & 0xF;
    uint32_t reg1 = (instr >> 12) & 0xF;

    uint32_t fp_reg = instr & 0xF;

    bool fp_low_bit = (instr >> 5) & 0x1;
    bool load = (instr >> 20) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (load)
    {
        if (is_double)
        {
            uint64_t d = vfp.get_reg64(fp_reg);

            cpu.set_register(reg2, d >> 32);
            cpu.set_register(reg1, d & 0xFFFFFFFF);
        }
        else
        {
            fp_reg <<= 1;
            fp_reg |= fp_low_bit;

            uint32_t s1 = vfp.get_reg32(fp_reg);
            uint32_t s2 = vfp.get_reg32(fp_reg + 1);

            cpu.set_register(reg2, s2);
            cpu.set_register(reg1, s1);
        }
    }
    else
    {
        if (is_double)
        {
            uint64_t d = cpu.get_register(reg1);
            d |= (uint64_t)cpu.get_register(reg2) << 32ULL;

            vfp.set_reg64(fp_reg, d);
        }
        else
        {
            fp_reg <<= 1;
            fp_reg |= fp_low_bit;

            uint32_t s1 = cpu.get_register(reg1);
            uint32_t s2 = cpu.get_register(reg2);

            vfp.set_reg32(fp_reg, s1);
            vfp.set_reg32(fp_reg + 1, s2);
        }
    }
}

void vfp_load_single(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    bool add_offset = (instr >> 23) & 0x1;
    bool low_bit = (instr >> 22) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t fp_reg = (instr >> 12) & 0xF;
    bool is_double = (instr >> 8) & 0x1;
    uint16_t offset = instr & 0xFF;
    offset <<= 2;

    uint32_t addr = cpu.get_register(base);

    if (add_offset)
        addr += offset;
    else
        addr -= offset;

    if (is_double)
        vfp.set_reg64(fp_reg, cpu.read64(addr));
    else
    {
        fp_reg <<= 1;
        fp_reg |= low_bit;

        vfp.set_reg32(fp_reg, cpu.read32(addr));
    }
}

void vfp_store_single(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    bool add_offset = (instr >> 23) & 0x1;
    bool low_bit = (instr >> 22) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t fp_reg = (instr >> 12) & 0xF;
    bool is_double = (instr >> 8) & 0x1;
    uint16_t offset = instr & 0xFF;
    offset <<= 2;

    uint32_t addr = cpu.get_register(base);

    if (add_offset)
        addr += offset;
    else
        addr -= offset;

    if (is_double)
        cpu.write64(addr, vfp.get_reg64(fp_reg));
    else
    {
        fp_reg <<= 1;
        fp_reg |= low_bit;

        cpu.write32(addr, vfp.get_reg32(fp_reg));
    }
}

void vfp_load_block(ARM_CPU& cpu, VFP &vfp, uint32_t instr)
{
    bool add_offset = (instr >> 23) & 0x1;
    bool low_bit = (instr >> 22) & 0x1;
    bool writeback = (instr >> 21) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    int start = (instr >> 12) & 0xF;
    bool is_double = (instr >> 8) & 0x1;
    uint16_t offset = instr & 0xFF;
    if (is_double)
        offset >>= 1;
    else
    {
        start <<= 1;
        start |= low_bit;
    }

    uint32_t addr = cpu.get_register(base);

    if (add_offset)
    {
        for (int i = start; i < start + offset; i++)
        {
            if (is_double)
            {
                vfp.set_reg64(i, cpu.read64(addr));
                addr += 8;
            }
            else
            {
                vfp.set_reg32(i, cpu.read32(addr));
                addr += 4;
            }
        }
    }
    else
    {
        for (int i = start + offset - 1; i >= start; i--)
        {
            if (is_double)
            {
                addr -= 8;
                vfp.set_reg64(i, cpu.read64(addr));
            }
            else
            {
                addr -= 4;
                vfp.set_reg32(i, cpu.read32(addr));
            }
        }
    }

    if (writeback)
        cpu.set_register(base, addr);
}

void vfp_store_block(ARM_CPU& cpu, VFP &vfp, uint32_t instr)
{
    bool add_offset = (instr >> 23) & 0x1;
    bool low_bit = (instr >> 22) & 0x1;
    bool writeback = (instr >> 21) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    int start = (instr >> 12) & 0xF;
    bool is_double = (instr >> 8) & 0x1;
    uint16_t offset = instr & 0xFF;
    if (is_double)
        offset >>= 1;
    else
    {
        start <<= 1;
        start |= low_bit;
    }

    uint32_t addr = cpu.get_register(base);

    if (add_offset)
    {
        for (int i = start; i < start + offset; i++)
        {
            if (is_double)
            {
                cpu.write64(addr, vfp.get_reg64(i));
                addr += 8;
            }
            else
            {
                cpu.write32(addr, vfp.get_reg32(i));
                addr += 4;
            }
        }
    }
    else
    {
        for (int i = start + offset - 1; i >= start; i--)
        {
            if (is_double)
            {
                addr -= 8;
                cpu.write64(addr, vfp.get_reg64(i));
            }
            else
            {
                addr -= 4;
                cpu.write32(addr, vfp.get_reg32(i));
            }
        }
    }

    if (writeback)
        cpu.set_register(base, addr);
}

void vfp_data_processing(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    if (!vfp.is_enabled())
    {
        cpu.und();
        return;
    }
    bool p = (instr >> 23) & 0x1;
    bool q = (instr >> 21) & 0x1;
    bool r = (instr >> 20) & 0x1;
    bool s = (instr >> 6) & 0x1;

    uint8_t op = (p << 3) | (q << 2) | (r << 1) | s;

    switch (op)
    {
        case 0x0:
            vfp_mac(cpu, vfp, instr);
            break;
        case 0x1:
            vfp_nmac(cpu, vfp, instr);
            break;
        case 0x2:
            vfp_msc(cpu, vfp, instr);
            break;
        case 0x3:
            vfp_nmsc(cpu, vfp, instr);
            break;
        case 0x4:
            vfp_mul(cpu, vfp, instr);
            break;
        case 0x5:
            vfp_nmul(cpu, vfp, instr);
            break;
        case 0x6:
            vfp_add(cpu, vfp, instr);
            break;
        case 0x7:
            vfp_sub(cpu, vfp, instr);
            break;
        case 0x8:
            vfp_div(cpu, vfp, instr);
            break;
        case 0xF:
            vfp_data_extended(cpu, vfp, instr);
            break;
        default:
            EmuException::die("[VFP_Interpreter] Unrecognized data processing op $%02X ($%08X)", op, instr);
    }
}

void vfp_mac(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(dest);
        double b = vfp.get_double(reg1);
        double c = vfp.get_double(reg2);

        vfp.set_double(dest, a + (b * c));
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(dest);
        float b = vfp.get_float(reg1);
        float c = vfp.get_float(reg2);

        vfp.set_float(dest, a + (b * c));
    }
}

void vfp_nmac(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(dest);
        double b = vfp.get_double(reg1);
        double c = vfp.get_double(reg2);

        vfp.set_double(dest, a - (b * c));
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(dest);
        float b = vfp.get_float(reg1);
        float c = vfp.get_float(reg2);

        vfp.set_float(dest, a - (b * c));
    }
}

void vfp_msc(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(dest);
        double b = vfp.get_double(reg1);
        double c = vfp.get_double(reg2);

        vfp.set_double(dest, -a + (b * c));
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(dest);
        float b = vfp.get_float(reg1);
        float c = vfp.get_float(reg2);

        vfp.set_float(dest, -a + (b * c));
    }
}

void vfp_nmsc(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(dest);
        double b = vfp.get_double(reg1);
        double c = vfp.get_double(reg2);

        vfp.set_double(dest, -a - (b * c));
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(dest);
        float b = vfp.get_float(reg1);
        float c = vfp.get_float(reg2);

        vfp.set_float(dest, -a - (b * c));
    }
}

void vfp_mul(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(reg1);
        double b = vfp.get_double(reg2);
        vfp.set_double(dest, a * b);
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(reg1);
        float b = vfp.get_float(reg2);
        vfp.set_float(dest, a * b);
    }
}

void vfp_nmul(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(reg1);
        double b = vfp.get_double(reg2);
        vfp.set_double(dest, -(a * b));
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(reg1);
        float b = vfp.get_float(reg2);
        vfp.set_float(dest, -(a * b));
    }
}

void vfp_add(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(reg1);
        double b = vfp.get_double(reg2);
        vfp.set_double(dest, a + b);
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(reg1);
        float b = vfp.get_float(reg2);
        vfp.set_float(dest, a + b);
    }
}

void vfp_sub(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(reg1);
        double b = vfp.get_double(reg2);
        vfp.set_double(dest, a - b);
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(reg1);
        float b = vfp.get_float(reg2);
        vfp.set_float(dest, a - b);
    }
}

void vfp_div(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(reg1);
        double b = vfp.get_double(reg2);
        vfp.set_double(dest, a / b);
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(reg1);
        float b = vfp.get_float(reg2);
        vfp.set_float(dest, a / b);
    }
}

void vfp_data_extended(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    uint8_t op = (instr >> 16) & 0xF;
    bool op_hi = (instr >> 7) & 0x1;
    op |= op_hi << 4;

    switch (op)
    {
        case 0x00:
            vfp_cpy(cpu, vfp, instr);
            break;
        case 0x01:
            vfp_neg(cpu, vfp, instr);
            break;
        case 0x04:
            vfp_cmps(cpu, vfp, instr);
            break;
        case 0x08:
            vfp_fuito(cpu, vfp, instr);
            break;
        case 0x0C:
            vfp_ftoui(cpu, vfp, instr);
            break;
        case 0x10:
            vfp_abs(cpu, vfp, instr);
            break;
        case 0x11:
            vfp_sqrt(cpu, vfp, instr);
            break;
        case 0x14:
            vfp_cmpes(cpu, vfp, instr);
            break;
        case 0x17:
            vfp_cvt(cpu, vfp, instr);
            break;
        case 0x18:
            vfp_fsito(cpu, vfp, instr);
            break;
        case 0x1C: //0x1C rounds towards zero
            vfp_ftoui(cpu, vfp, instr);
            break;
        case 0x1D:
            vfp_ftosi(cpu, vfp, instr);
            break;
        default:
            EmuException::die("[VFP_Interpreter] Unrecognized data extended op $%02X ($%08X)", op, instr);
    }
}

void vfp_cpy(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        uint64_t source_reg = vfp.get_reg64(source);
        vfp.set_reg64(dest, source_reg);
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        uint32_t source_reg = vfp.get_reg32(source);
        vfp.set_reg32(dest, source_reg);
    }
}

void vfp_neg(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double source_reg = vfp.get_double(source);
        vfp.set_double(dest, -source_reg);
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        float source_reg = vfp.get_float(source);
        vfp.set_float(dest, -source_reg);
    }
}

void vfp_abs(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        uint64_t source_reg = vfp.get_reg64(source);
        source_reg &= ~(1ULL << 63ULL);
        vfp.set_reg64(dest, source_reg);
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        uint32_t source_reg = vfp.get_reg32(source);
        source_reg &= ~(1 << 31);
        vfp.set_reg32(dest, source_reg);
    }
}

void vfp_cmps(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    //TODO: CMPS should signal an exception if an operand is a signalling NaN
    int reg1 = (instr >> 12) & 0xF;
    int reg2 = instr & 0xF;
    bool reg1_low_bit = (instr >> 22) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(reg1);
        double b = vfp.get_double(reg2);
        vfp.cmp(a, b);
    }
    else
    {
        reg1 <<= 1;
        reg2 <<= 1;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(reg1);
        float b = vfp.get_float(reg2);
        vfp.cmp(a, b);
    }
}

void vfp_cmpes(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    //TODO: CMPES should signal an exception if an operand is any NaN
    int reg1 = (instr >> 12) & 0xF;
    int reg2 = instr & 0xF;
    bool reg1_low_bit = (instr >> 22) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double a = vfp.get_double(reg1);
        double b = vfp.get_double(reg2);
        vfp.cmp(a, b);
    }
    else
    {
        reg1 <<= 1;
        reg2 <<= 1;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;

        float a = vfp.get_float(reg1);
        float b = vfp.get_float(reg2);
        vfp.cmp(a, b);
    }
}

void vfp_cvt(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int reg1 = (instr >> 12) & 0xF;
    int reg2 = instr & 0xF;
    bool reg1_low_bit = (instr >> 22) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        reg1 <<= 1;
        reg1 |= reg1_low_bit;

        double source_reg = vfp.get_double(reg2);
        vfp.set_float(reg1, source_reg);
    }
    else
    {
        reg2 <<= 1;
        reg2 |= reg2_low_bit;

        float source_reg = vfp.get_float(reg2);
        vfp.set_double(reg1, source_reg);
    }
}

void vfp_sqrt(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double source_reg = vfp.get_double(source);
        vfp.set_double(dest, sqrt(source_reg));
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        float source_reg = vfp.get_float(source);
        vfp.set_float(dest, sqrtf(source_reg));
    }
}

void vfp_fuito(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double source_reg = (double)vfp.get_reg64(source);
        vfp.set_double(dest, source_reg);
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        float source_reg = (float)vfp.get_reg32(source);
        vfp.set_float(dest, source_reg);
    }
}

void vfp_fsito(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        double source_reg = (double)(int64_t)vfp.get_reg64(source);
        vfp.set_double(dest, source_reg);
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        float source_reg = (float)(int32_t)vfp.get_reg32(source);
        vfp.set_float(dest, source_reg);
    }
}

void vfp_ftoui(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        uint64_t source_reg = (uint64_t)vfp.get_double(source);
        vfp.set_reg64(dest, source_reg);
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        uint32_t source_reg = (uint32_t)vfp.get_float(source);
        vfp.set_reg32(dest, source_reg);
    }
}

void vfp_ftosi(ARM_CPU &cpu, VFP &vfp, uint32_t instr)
{
    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    if (is_double)
    {
        //TODO: Does passing in an int64_t to a function using uint64_t harm anything?
        int64_t source_reg = (int64_t)vfp.get_double(source);
        vfp.set_reg64(dest, source_reg);
    }
    else
    {
        dest <<= 1;
        dest |= dest_low_bit;
        source <<= 1;
        source |= source_low_bit;

        int32_t source_reg = (int32_t)vfp.get_float(source);
        vfp.set_reg32(dest, source_reg);
    }
}

};
