#include "../common/common.hpp"
#include "signextend.hpp"
#include "xtensa.hpp"
#include "xtensa_interpreter.hpp"

namespace Xtensa_Interpreter
{

const static int b4const[16] =
{
    -1,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    10,
    12,
    16,
    32,
    64,
    128,
    256
};

const static uint32_t b4constu[16] =
{
    0x8000,
    0x10000,
    0x2,
    0x3,
    0x4,
    0x5,
    0x6,
    0x7,
    0x8,
    0xA,
    0xC,
    0x10,
    0x20,
    0x40,
    0x80,
    0x100
};

inline void make_24bit(Xtensa& cpu, uint32_t& instr)
{
    instr |= cpu.fetch_byte() << 16;
}

void interpret(Xtensa &cpu, uint32_t instr)
{
    int op0 = instr & 0xF;
    switch (op0)
    {
        case 0x0:
            op0_qrst(cpu, instr);
            break;
        case 0x1:
            l32r(cpu, instr);
            break;
        case 0x2:
            op0_lsai(cpu, instr);
            break;
        case 0x5:
            op0_calln(cpu, instr);
            break;
        case 0x6:
            op0_si(cpu, instr);
            break;
        case 0x7:
            op0_b(cpu, instr);
            break;
        case 0x8:
            l32i_n(cpu, instr);
            break;
        case 0x9:
            s32i_n(cpu, instr);
            break;
        case 0xA:
            add_n(cpu, instr);
            break;
        case 0xB:
            addi_n(cpu, instr);
            break;
        case 0xC:
            op0_st2(cpu, instr);
            break;
        case 0xD:
            op0_st3(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0 $%02X ($%04X)", op0, instr);
    }
}

void l32r(Xtensa &cpu, uint32_t instr)
{
    make_24bit(cpu, instr);

    int gpr = (instr >> 4) & 0xF;
    int offset = -(0x10000 - (instr >> 8));
    offset <<= 2;

    uint32_t addr;
    if (cpu.extended_l32r())
        addr = cpu.get_litbase() + offset;
    else
        addr = (cpu.get_pc() + offset) & ~0x3;
    uint32_t value = cpu.read32(addr);

    cpu.set_gpr(gpr, value);
}

void l32i_n(Xtensa &cpu, uint32_t instr)
{
    int offset = (instr >> 12) << 2;
    int base = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t addr = cpu.get_gpr(base) + offset;
    uint32_t value = cpu.read32(addr);
    cpu.set_gpr(dest, value);
}

void s32i_n(Xtensa &cpu, uint32_t instr)
{
    int offset = (instr >> 12) << 2;
    int base = (instr >> 8) & 0xF;
    int source = (instr >> 4) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    uint32_t addr = cpu.get_gpr(base) + offset;
    cpu.write32(addr, source_reg);
}

void add_n(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, source1 + source2);
}

void addi_n(Xtensa &cpu, uint32_t instr)
{
    int imm = (instr >> 4) & 0xF;
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    if (!imm)
        cpu.set_gpr(dest, source_reg - 1);
    else
        cpu.set_gpr(dest, source_reg + imm);
}

void op0_qrst(Xtensa &cpu, uint32_t instr)
{
    make_24bit(cpu, instr);
    int op1 = (instr >> 16) & 0xF;
    switch (op1)
    {
        case 0x0:
            op0_qrst_rst0(cpu, instr);
            break;
        case 0x1:
            op0_qrst_rst1(cpu, instr);
            break;
        case 0x2:
            op0_qrst_rst2(cpu, instr);
            break;
        case 0x3:
            op0_qrst_rst3(cpu, instr);
            break;
        case 0x4:
        case 0x5:
            extui(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst $%02X ($%06X)", op1, instr);
    }
}

void extui(Xtensa &cpu, uint32_t instr)
{
    int shift = (instr >> 8) & 0xF;
    shift |= ((instr >> 16) & 0x1) << 4;
    int maskimm = (instr >> 20) + 1;
    int source = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t mask = 0;
    for (int i = 0; i < maskimm; i++)
        mask |= 1 << i;

    uint32_t source_reg = cpu.get_gpr(source);
    source_reg >>= shift;
    cpu.set_gpr(dest, source_reg & mask);
}

void op0_qrst_rst0(Xtensa &cpu, uint32_t instr)
{
    int op2 = instr >> 20;
    switch (op2)
    {
        case 0x0:
            op0_qrst_rst0_st0(cpu, instr);
            break;
        case 0x1:
            and_(cpu, instr);
            break;
        case 0x2:
            or_(cpu, instr);
            break;
        case 0x3:
            xor_(cpu, instr);
            break;
        case 0x4:
            op0_qrst_rst0_st1(cpu, instr);
            break;
        case 0x5:
            //TLB ops, treat as NOP
            break;
        case 0x6:
            op0_qrst_rst0_rt0(cpu, instr);
            break;
        case 0x8:
            add(cpu, instr);
            break;
        case 0x9:
            addx2(cpu, instr);
            break;
        case 0xA:
            addx4(cpu, instr);
            break;
        case 0xB:
            addx8(cpu, instr);
            break;
        case 0xC:
            sub(cpu, instr);
            break;
        case 0xD:
            subx2(cpu, instr);
            break;
        case 0xE:
            subx4(cpu, instr);
            break;
        case 0xF:
            subx8(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst0 $%02X ($%06X)", op2, instr);
    }
}

void and_(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, source1 & source2);
}

void or_(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, source1 | source2);
}

void xor_(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, source1 ^ source2);
}

void add(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, source1 + source2);
}

void addx2(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, (source1 << 1) + source2);
}

void addx4(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, (source1 << 2) + source2);
}

void addx8(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, (source1 << 3) + source2);
}

void sub(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);
    cpu.set_gpr(dest, source1 - source2);
}

void subx2(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t value1 = cpu.get_gpr(reg1) << 1;
    uint32_t value2 = cpu.get_gpr(reg2);

    cpu.set_gpr(dest, value1 - value2);
}

void subx4(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t value1 = cpu.get_gpr(reg1) << 2;
    uint32_t value2 = cpu.get_gpr(reg2);

    cpu.set_gpr(dest, value1 - value2);
}

void subx8(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t value1 = cpu.get_gpr(reg1) << 3;
    uint32_t value2 = cpu.get_gpr(reg2);

    cpu.set_gpr(dest, value1 - value2);
}

void op0_qrst_rst0_st0(Xtensa &cpu, uint32_t instr)
{
    int r = (instr >> 12) & 0xF;
    switch (r)
    {
        case 0x0:
            op0_qrst_rst0_st0_snm0(cpu, instr);
            break;
        case 0x2:
            //Sync ops, treat as NOP
            break;
        case 0x3:
            op0_qrst_rst0_st0_rfei(cpu, instr);
            break;
        case 0x6:
            rsil(cpu, instr);
            break;
        case 0x7:
            waiti(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst0_st0 $%02X ($%06X)", r, instr);
    }
}

void rsil(Xtensa &cpu, uint32_t instr)
{
    //Read and set interrupt level
    int level = (instr >> 8) & 0xF;
    int gpr = (instr >> 4) & 0xF;

    uint32_t ps = cpu.get_ps();
    cpu.set_gpr(gpr, ps & 0xF);

    ps &= ~0xF;
    cpu.set_ps(ps | level);
}

void waiti(Xtensa &cpu, uint32_t instr)
{
    int level = (instr >> 8) & 0xF;

    uint32_t ps = cpu.get_ps();
    ps &= ~0xF;
    cpu.set_ps(ps | level);
    cpu.halt();
}

void op0_qrst_rst0_st0_snm0(Xtensa &cpu, uint32_t instr)
{
    int mn = (instr >> 4) & 0xF;
    switch (mn)
    {
        case 0xA:
            jx(cpu, instr);
            break;
        case 0xD:
            callx4(cpu, instr);
            break;
        case 0xE:
            callx8(cpu, instr);
            break;
        case 0xF:
            callx12(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst0_st0_snm0 $%02X ($%06X)", mn, instr);
    }
}

void jx(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;
    cpu.jp(cpu.get_gpr(reg));
}

void callx4(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;
    cpu.windowed_call(cpu.get_gpr(reg), 1);
}

void callx8(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;
    cpu.windowed_call(cpu.get_gpr(reg), 2);
}

void callx12(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;
    cpu.windowed_call(cpu.get_gpr(reg), 3);
}

void op0_qrst_rst0_st0_rfei(Xtensa &cpu, uint32_t instr)
{
    int t = (instr >> 4) & 0xF;
    switch (t)
    {
        case 0x1:
            rfi(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst0_st0_rfei $%02X ($%06X)", t, instr);
    }
}

void rfi(Xtensa &cpu, uint32_t instr)
{
    int level = ((instr >> 8) & 0xF) - 1;
    cpu.rfi(level);
}

void op0_qrst_rst0_rt0(Xtensa &cpu, uint32_t instr)
{
    int s = (instr >> 8) & 0xF;
    switch (s)
    {
        case 0x0:
            neg(cpu, instr);
            break;
        case 0x1:
            abs(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst0_rt0 $%02X ($%06X)", s, instr);
    }
}

void neg(Xtensa &cpu, uint32_t instr)
{
    int source = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, ~source_reg + 1);
}

void abs(Xtensa &cpu, uint32_t instr)
{
    int source = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg & ~(1 << 31));
}

void op0_qrst_rst0_st1(Xtensa &cpu, uint32_t instr)
{
    int r = (instr >> 12) & 0xF;
    switch (r)
    {
        case 0x0:
            ssr(cpu, instr);
            break;
        case 0x1:
            ssl(cpu, instr);
            break;
        case 0x2:
            ssa8l(cpu, instr);
            break;
        case 0x4:
            ssai(cpu, instr);
            break;
        case 0xF:
            nsau(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst0_st1 $%02X ($%06X)", r, instr);
    }
}

void ssr(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;

    uint32_t value = cpu.get_gpr(reg) & 0x1F;
    cpu.set_sar(value);
}

void ssl(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;

    uint32_t value = cpu.get_gpr(reg) & 0x1F;
    cpu.set_sar(32 - value);
}

void ssa8l(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;

    uint32_t value = (cpu.get_gpr(reg) & 0x3) << 3;
    cpu.set_sar(value);
}

void ssai(Xtensa &cpu, uint32_t instr)
{
    uint32_t value = (instr >> 8) & 0xF;
    value |= ((instr >> 4) & 0x1) << 4;
    cpu.set_sar(value);
}

void nsau(Xtensa &cpu, uint32_t instr)
{
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t value = cpu.get_gpr(source);

    if (!value)
        cpu.set_gpr(dest, 32);
    else
    {
        int sa = 0;
        for (; sa < 32; sa++)
        {
            if (value & (1 << (31 - sa)))
                break;
        }

        cpu.set_gpr(dest, sa);
    }
}

void op0_qrst_rst1(Xtensa &cpu, uint32_t instr)
{
    int op2 = instr >> 20;
    switch (op2)
    {
        case 0x0:
        case 0x1:
            slli(cpu, instr);
            break;
        case 0x2:
        case 0x3:
            srai(cpu, instr);
            break;
        case 0x4:
            srli(cpu, instr);
            break;
        case 0x6:
            xsr(cpu, instr);
            break;
        case 0x8:
            src(cpu, instr);
            break;
        case 0x9:
            srl(cpu, instr);
            break;
        case 0xA:
            sll(cpu, instr);
            break;
        case 0xB:
            sra(cpu, instr);
            break;
        case 0xC:
            mul16u(cpu, instr);
            break;
        case 0xD:
            mul16s(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst1 $%02X ($%06X)", op2, instr);
    }
}

void slli(Xtensa &cpu, uint32_t instr)
{
    int shift = (instr >> 4) & 0xF;
    shift |= ((instr >> 20) & 0x1) << 4;
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg << (32 - shift));
}

void srai(Xtensa &cpu, uint32_t instr)
{
    int shift = (instr >> 8) & 0xF;
    shift |= ((instr >> 20) & 0x1) << 4;
    int source = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    int32_t source_reg = (int32_t)cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg >> shift);
}

void srli(Xtensa &cpu, uint32_t instr)
{
    int shift = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;
    int source = (instr >> 4) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg >> shift);
}

void xsr(Xtensa &cpu, uint32_t instr)
{
    int xsr = (instr >> 8) & 0xFF;
    int gpr = (instr >> 4) & 0xF;

    uint32_t xsr_value = cpu.get_xsr(xsr);
    uint32_t gpr_value = cpu.get_gpr(gpr);

    cpu.set_xsr(xsr, gpr_value);
    cpu.set_gpr(gpr, xsr_value);
}

void src(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 4) & 0xF;
    int reg2 = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint64_t value = cpu.get_gpr(reg2);
    value <<= 32ULL;
    value |= cpu.get_gpr(reg1);
    cpu.set_gpr(dest, value >> cpu.get_sar());
}

void srl(Xtensa &cpu, uint32_t instr)
{
    int source = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg >> cpu.get_sar());
}

void sll(Xtensa &cpu, uint32_t instr)
{
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg << (32 - cpu.get_sar()));
}

void sra(Xtensa &cpu, uint32_t instr)
{
    int source = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    int32_t source_reg = (int32_t)cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg >> cpu.get_sar());
}

void mul16u(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t value1 = cpu.get_gpr(reg1) & 0xFFFF;
    uint32_t value2 = cpu.get_gpr(reg2) & 0xFFFF;

    cpu.set_gpr(dest, value1 * value2);
}

void mul16s(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    int32_t value1 = (int16_t)(cpu.get_gpr(reg1) & 0xFFFF);
    int32_t value2 = (int16_t)(cpu.get_gpr(reg2) & 0xFFFF);

    cpu.set_gpr(dest, value1 * value2);
}

void op0_qrst_rst2(Xtensa &cpu, uint32_t instr)
{
    int op2 = instr >> 20;
    switch (op2)
    {
        case 0x8:
            mull(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst2 $%02X ($%06X)", op2, instr);
    }
}

void mull(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int dest = (instr >> 12) & 0xF;

    //TODO: Xtensa manual says that this is a 32-bit multiplication, but it also says the 32 LSB of the product
    //are written to dest. Is this code correct?
    uint64_t value1 = cpu.get_gpr(reg1);
    uint64_t value2 = cpu.get_gpr(reg2);

    cpu.set_gpr(dest, (value1 * value2) & 0xFFFFFFFF);
}

void op0_qrst_rst3(Xtensa &cpu, uint32_t instr)
{
    int op2 = instr >> 20;
    switch (op2)
    {
        case 0x0:
            rsr(cpu, instr);
            break;
        case 0x1:
            wsr(cpu, instr);
            break;
        case 0x4:
            min_(cpu, instr);
            break;
        case 0x5:
            max_(cpu, instr);
            break;
        case 0x6:
            minu(cpu, instr);
            break;
        case 0x7:
            maxu(cpu, instr);
            break;
        case 0x8:
            moveqz(cpu, instr);
            break;
        case 0x9:
            movnez(cpu, instr);
            break;
        case 0xA:
            movltz(cpu, instr);
            break;
        case 0xB:
            movgez(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_qrst_rst3 $%02X ($%06X)", op2, instr);
    }
}

void rsr(Xtensa &cpu, uint32_t instr)
{
    int gpr = (instr >> 4) & 0xF;
    int xsr = (instr >> 8) & 0xFF;

    uint32_t value = cpu.get_xsr(xsr);
    cpu.set_gpr(gpr, value);
}

void wsr(Xtensa &cpu, uint32_t instr)
{
    int gpr = (instr >> 4) & 0xF;
    int xsr = (instr >> 8) & 0xFF;

    uint32_t value = cpu.get_gpr(gpr);
    cpu.set_xsr(xsr, value);
}

void min_(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 4) & 0xF;
    int reg2 = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    int32_t value1 = (int32_t)cpu.get_gpr(reg1);
    int32_t value2 = (int32_t)cpu.get_gpr(reg2);

    cpu.set_gpr(dest, (value1 < value2) ? value1 : value2);
}

void max_(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 4) & 0xF;
    int reg2 = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    int32_t value1 = (int32_t)cpu.get_gpr(reg1);
    int32_t value2 = (int32_t)cpu.get_gpr(reg2);

    cpu.set_gpr(dest, (value1 > value2) ? value1 : value2);
}

void minu(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 4) & 0xF;
    int reg2 = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t value1 = cpu.get_gpr(reg1);
    uint32_t value2 = cpu.get_gpr(reg2);

    cpu.set_gpr(dest, (value1 < value2) ? value1 : value2);
}

void maxu(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 4) & 0xF;
    int reg2 = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    uint32_t value1 = cpu.get_gpr(reg1);
    uint32_t value2 = cpu.get_gpr(reg2);

    cpu.set_gpr(dest, (value1 > value2) ? value1 : value2);
}

void moveqz(Xtensa &cpu, uint32_t instr)
{
    int test = (instr >> 4) & 0xF;
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    if (cpu.get_gpr(test) == 0)
    {
        uint32_t source_reg = cpu.get_gpr(source);
        cpu.set_gpr(dest, source_reg);
    }
}

void movnez(Xtensa &cpu, uint32_t instr)
{
    int test = (instr >> 4) & 0xF;
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    if (cpu.get_gpr(test) != 0)
    {
        uint32_t source_reg = cpu.get_gpr(source);
        cpu.set_gpr(dest, source_reg);
    }
}

void movltz(Xtensa &cpu, uint32_t instr)
{
    int test = (instr >> 4) & 0xF;
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    if ((int32_t)cpu.get_gpr(test) < 0)
    {
        uint32_t source_reg = cpu.get_gpr(source);
        cpu.set_gpr(dest, source_reg);
    }
}

void movgez(Xtensa &cpu, uint32_t instr)
{
    int test = (instr >> 4) & 0xF;
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 12) & 0xF;

    if ((int32_t)cpu.get_gpr(test) >= 0)
    {
        uint32_t source_reg = cpu.get_gpr(source);
        cpu.set_gpr(dest, source_reg);
    }
}

void op0_lsai(Xtensa &cpu, uint32_t instr)
{
    make_24bit(cpu, instr);
    int r = (instr >> 12) & 0xF;
    switch (r)
    {
        case 0x0:
            l8ui(cpu, instr);
            break;
        case 0x1:
            l16ui(cpu, instr);
            break;
        case 0x2:
            l32i(cpu, instr);
            break;
        case 0x4:
            s8i(cpu, instr);
            break;
        case 0x5:
            s16i(cpu, instr);
            break;
        case 0x6:
            s32i(cpu, instr);
            break;
        case 0x9:
            l16si(cpu, instr);
            break;
        case 0xA:
            movi(cpu, instr);
            break;
        case 0xC:
            addi(cpu, instr);
            break;
        case 0xD:
            addmi(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_lsai $%02X ($%06X)", r, instr);
    }
}

void l8ui(Xtensa &cpu, uint32_t instr)
{
    int offset = instr >> 16;
    int base = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t addr = cpu.get_gpr(base) + offset;
    uint32_t value = cpu.read8(addr);
    cpu.set_gpr(dest, value);
}

void l16ui(Xtensa &cpu, uint32_t instr)
{
    int offset = (instr >> 16) << 1;
    int base = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t addr = cpu.get_gpr(base) + offset;
    uint16_t value = cpu.read16(addr);
    cpu.set_gpr(dest, value);
}

void l32i(Xtensa &cpu, uint32_t instr)
{
    int offset = (instr >> 16) << 2;
    int base = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t addr = cpu.get_gpr(base) + offset;
    uint32_t value = cpu.read32(addr);
    cpu.set_gpr(dest, value);
}

void s8i(Xtensa &cpu, uint32_t instr)
{
    int offset = instr >> 16;
    int base = (instr >> 8) & 0xF;
    int source = (instr >> 4) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    uint32_t addr = cpu.get_gpr(base) + offset;
    cpu.write8(addr, source_reg);
}

void s16i(Xtensa &cpu, uint32_t instr)
{
    int offset = (instr >> 16) << 1;
    int base = (instr >> 8) & 0xF;
    int source = (instr >> 4) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    uint32_t addr = cpu.get_gpr(base) + offset;
    cpu.write16(addr, source_reg);
}

void s32i(Xtensa &cpu, uint32_t instr)
{
    int offset = (instr >> 16) << 2;
    int base = (instr >> 8) & 0xF;
    int source = (instr >> 4) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    uint32_t addr = cpu.get_gpr(base) + offset;
    cpu.write32(addr, source_reg);
}

void l16si(Xtensa &cpu, uint32_t instr)
{
    int offset = (instr >> 16) << 1;
    int base = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t addr = cpu.get_gpr(base) + offset;
    int16_t value = (int16_t)cpu.read16(addr);
    cpu.set_gpr(dest, value);
}

void movi(Xtensa &cpu, uint32_t instr)
{
    int imm = instr >> 16;
    imm |= instr & 0xF00;
    imm = SignExtend<12>(imm);
    int dest = (instr >> 4) & 0xF;

    cpu.set_gpr(dest, imm);
}

void addi(Xtensa &cpu, uint32_t instr)
{
    int imm = SignExtend<8>(instr >> 16);
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg + imm);
}

void addmi(Xtensa &cpu, uint32_t instr)
{
    int imm = SignExtend<8>(instr >> 16) << 8;
    int source = (instr >> 8) & 0xF;
    int dest = (instr >> 4) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg + imm);
}

void op0_calln(Xtensa &cpu, uint32_t instr)
{
    make_24bit(cpu, instr);
    int t = (instr >> 4) & 0xF;
    switch (t)
    {
        case 0x0:
        case 0x4:
        case 0x8:
        case 0xC:
            call0(cpu, instr);
            break;
        case 0x1:
        case 0x5:
        case 0x9:
        case 0xD:
            call4(cpu, instr);
            break;
        case 0x2:
        case 0x6:
        case 0xA:
        case 0xE:
            call8(cpu, instr);
            break;
        case 0x3:
        case 0x7:
        case 0xB:
        case 0xF:
            call12(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_calln $%02X ($%06X)", t, instr);
    }
}

void call0(Xtensa &cpu, uint32_t instr)
{
    uint32_t pc = cpu.get_pc();
    cpu.set_gpr(0, pc);

    uint32_t addr = (pc - 3) & ~0x3;
    int offset = SignExtend<18, int>(instr >> 6) << 2;
    addr += offset;

    cpu.jp(addr + 4);
}

void call4(Xtensa &cpu, uint32_t instr)
{
    uint32_t addr = (cpu.get_pc() - 3) & ~0x3;
    int offset = SignExtend<18, int>(instr >> 6) << 2;
    addr += offset;

    cpu.windowed_call(addr + 4, 1);
}

void call8(Xtensa &cpu, uint32_t instr)
{
    uint32_t addr = (cpu.get_pc() - 3) & ~0x3;
    int offset = SignExtend<18, int>(instr >> 6) << 2;
    addr += offset;

    cpu.windowed_call(addr + 4, 2);
}

void call12(Xtensa &cpu, uint32_t instr)
{
    uint32_t addr = (cpu.get_pc() - 3) & ~0x3;
    int offset = SignExtend<18, int>(instr >> 6) << 2;
    addr += offset;

    cpu.windowed_call(addr + 4, 3);
}

void op0_si(Xtensa &cpu, uint32_t instr)
{
    make_24bit(cpu, instr);
    int t = (instr >> 4) & 0xF;
    switch (t)
    {
        case 0x0:
        case 0x4:
        case 0x8:
        case 0xC:
            j(cpu, instr);
            break;
        case 0x1:
            beqz(cpu, instr);
            break;
        case 0x2:
            beqi(cpu, instr);
            break;
        case 0x3:
            entry(cpu, instr);
            break;
        case 0x5:
            bnez(cpu, instr);
            break;
        case 0x6:
            bnei(cpu, instr);
            break;
        case 0x7:
            op0_si_b1(cpu, instr);
            break;
        case 0x9:
            bltz(cpu, instr);
            break;
        case 0xA:
            blti(cpu, instr);
            break;
        case 0xB:
            bltui(cpu, instr);
            break;
        case 0xD:
            bgez(cpu, instr);
            break;
        case 0xE:
            bgei(cpu, instr);
            break;
        case 0xF:
            bgeui(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_si $%02X ($%06X)", t, instr);
    }
}

void j(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<18>(instr >> 6);

    cpu.branch(offset + 1);
}

void beqz(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<12>(instr >> 12);
    int reg = (instr >> 8) & 0xF;

    uint32_t value = cpu.get_gpr(reg);
    if (!value)
        cpu.branch(offset + 1);
}

void beqi(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    int imm = (instr >> 12) & 0xF;
    int reg = (instr >> 8) & 0xF;

    imm = b4const[imm];
    int value = SignExtend<32>(cpu.get_gpr(reg));

    if (value == imm)
        cpu.branch(offset + 1);
}

void entry(Xtensa &cpu, uint32_t instr)
{
    int sp = (instr >> 8) & 0xF;
    int frame = (instr >> 12) << 3;

    cpu.entry(sp, frame);
}

void bnez(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<12>(instr >> 12);
    int reg = (instr >> 8) & 0xF;

    uint32_t value = cpu.get_gpr(reg);
    if (value)
        cpu.branch(offset + 1);
}

void bnei(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    int imm = (instr >> 12) & 0xF;
    int reg = (instr >> 8) & 0xF;

    imm = b4const[imm];
    int value = SignExtend<32>(cpu.get_gpr(reg));

    if (value != imm)
        cpu.branch(offset + 1);
}

void bltz(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<12>(instr >> 12);
    int reg = (instr >> 8) & 0xF;

    int32_t value = (int32_t)cpu.get_gpr(reg);
    if (value < 0)
        cpu.branch(offset + 1);
}

void blti(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    int32_t imm = (instr >> 12) & 0xF;
    int reg = (instr >> 8) & 0xF;

    imm = b4const[imm];
    int32_t value = (int32_t)cpu.get_gpr(reg);

    if (value < imm)
        cpu.branch(offset + 1);
}

void bltui(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    uint32_t imm = (instr >> 12) & 0xF;
    int reg = (instr >> 8) & 0xF;

    imm = b4constu[imm];
    uint32_t value = cpu.get_gpr(reg);

    if (value < imm)
        cpu.branch(offset + 1);
}

void bgez(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<12>(instr >> 12);
    int reg = (instr >> 8) & 0xF;

    int32_t value = (int32_t)cpu.get_gpr(reg);
    if (value >= 0)
        cpu.branch(offset + 1);
}

void bgei(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    int imm = (instr >> 12) & 0xF;
    int reg = (instr >> 8) & 0xF;

    imm = b4const[imm];
    int value = SignExtend<32>(cpu.get_gpr(reg));

    if (value >= imm)
        cpu.branch(offset + 1);
}

void bgeui(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    uint32_t imm = (instr >> 12) & 0xF;
    int reg = (instr >> 8) & 0xF;

    imm = b4constu[imm];
    uint32_t value = cpu.get_gpr(reg);

    if (value >= imm)
        cpu.branch(offset + 1);
}

void op0_si_b1(Xtensa &cpu, uint32_t instr)
{
    int r = (instr >> 12) & 0xF;
    switch (r)
    {
        case 0x9:
            loopnez(cpu, instr);
            break;
        case 0xA:
            loopgtz(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_si_b1 $%02X ($%06X)", r, instr);
    }
}

void loopnez(Xtensa &cpu, uint32_t instr)
{
    int offset = instr >> 16;
    int count_reg = (instr >> 8) & 0xF;

    int count = SignExtend<32>(cpu.get_gpr(count_reg));

    cpu.setup_loop(count, offset, count != 0);
}

void loopgtz(Xtensa &cpu, uint32_t instr)
{
    int offset = instr >> 16;
    int count_reg = (instr >> 8) & 0xF;

    int count = SignExtend<32>(cpu.get_gpr(count_reg));

    cpu.setup_loop(count, offset, count > 0);
}

void op0_b(Xtensa &cpu, uint32_t instr)
{
    make_24bit(cpu, instr);
    int r = (instr >> 12) & 0xF;
    switch (r)
    {
        case 0x0:
            bnone(cpu, instr);
            break;
        case 0x1:
            beq(cpu, instr);
            break;
        case 0x2:
            blt(cpu, instr);
            break;
        case 0x3:
            bltu(cpu, instr);
            break;
        case 0x5:
            bbc(cpu, instr);
            break;
        case 0x6:
        case 0x7:
            bbci(cpu, instr);
            break;
        case 0x8:
            bany(cpu, instr);
            break;
        case 0x9:
            bne(cpu, instr);
            break;
        case 0xA:
            bge(cpu, instr);
            break;
        case 0xB:
            bgeu(cpu, instr);
            break;
        case 0xE:
        case 0xF:
            bbsi(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_b $%02X ($%06X)", r, instr);
    }
}

void bnone(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    uint32_t value = cpu.get_gpr(reg1);
    uint32_t mask = cpu.get_gpr(reg2);

    if (!(value & mask))
        cpu.branch(offset + 1);
}

void beq(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);

    if (source1 == source2)
        cpu.branch(offset + 1);
}

void blt(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    int32_t source1 = (int32_t)cpu.get_gpr(reg1);
    int32_t source2 = (int32_t)cpu.get_gpr(reg2);

    if (source1 < source2)
        cpu.branch(offset + 1);
}

void bltu(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);

    if (source1 < source2)
        cpu.branch(offset + 1);
}

void bbc(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    int reg = (instr >> 8) & 0xF;
    int test = (instr >> 4) & 0xF;

    uint32_t value = cpu.get_gpr(reg);
    uint32_t imm = cpu.get_gpr(test) & 0x1F;

    if (!(value & (1 << imm)))
        cpu.branch(offset + 1);
}

void bbci(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    int reg = (instr >> 8) & 0xF;
    int imm = (instr >> 4) & 0xF;
    imm |= ((instr >> 12) & 0x1) << 4;

    uint32_t value = cpu.get_gpr(reg);
    if (!(value & (1 << imm)))
        cpu.branch(offset + 1);
}

void bany(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    uint32_t value = cpu.get_gpr(reg1);
    uint32_t mask = cpu.get_gpr(reg2);

    if (value & mask)
        cpu.branch(offset + 1);
}

void bne(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);

    if (source1 != source2)
        cpu.branch(offset + 1);
}

void bge(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    int32_t source1 = (int32_t)cpu.get_gpr(reg1);
    int32_t source2 = (int32_t)cpu.get_gpr(reg2);

    if (source1 >= source2)
        cpu.branch(offset + 1);
}

void bgeu(Xtensa &cpu, uint32_t instr)
{
    int reg1 = (instr >> 8) & 0xF;
    int reg2 = (instr >> 4) & 0xF;
    int offset = SignExtend<8>(instr >> 16);

    uint32_t source1 = cpu.get_gpr(reg1);
    uint32_t source2 = cpu.get_gpr(reg2);

    if (source1 >= source2)
        cpu.branch(offset + 1);
}

void bbsi(Xtensa &cpu, uint32_t instr)
{
    int offset = SignExtend<8>(instr >> 16);
    int reg = (instr >> 8) & 0xF;
    int imm = (instr >> 4) & 0xF;
    imm |= ((instr >> 12) & 0x1) << 4;

    uint32_t value = cpu.get_gpr(reg);
    if (value & (1 << imm))
        cpu.branch(offset + 1);
}

void op0_st2(Xtensa &cpu, uint32_t instr)
{
    switch ((instr >> 4) & 0xF)
    {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
            movi_n(cpu, instr);
            break;
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
            beqz_n(cpu, instr);
            break;
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
            bnez_n(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_st2 $%02X ($%04X)", (instr >> 4) & 0xF, instr);
    }
}

void movi_n(Xtensa &cpu, uint32_t instr)
{
    int reg = (instr >> 8) & 0xF;
    int imm = (instr >> 12) & 0xF;
    imm |= instr & 0x70;

    if (imm >= 96)
        imm = -(128 - imm);

    cpu.set_gpr(reg, imm);
}

void beqz_n(Xtensa &cpu, uint32_t instr)
{
    int offset = instr >> 12;
    offset |= instr & 0x30;
    int reg = (instr >> 8) & 0xF;

    uint32_t value = cpu.get_gpr(reg);

    if (!value)
        cpu.branch(offset + 2);
}

void bnez_n(Xtensa &cpu, uint32_t instr)
{
    int offset = instr >> 12;
    offset |= instr & 0x30;
    int reg = (instr >> 8) & 0xF;

    uint32_t value = cpu.get_gpr(reg);

    if (value)
        cpu.branch(offset + 2);
}

void op0_st3(Xtensa &cpu, uint32_t instr)
{
    int r = (instr >> 12) & 0xF;
    switch (r)
    {
        case 0x0:
            mov(cpu, instr);
            break;
        case 0xF:
            op0_st3_s3(cpu, instr);
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_st3 $%02X ($%04X)", r, instr);
    }
}

void mov(Xtensa &cpu, uint32_t instr)
{
    int dest = (instr >> 4) & 0xF;
    int source = (instr >> 8) & 0xF;

    uint32_t source_reg = cpu.get_gpr(source);
    cpu.set_gpr(dest, source_reg);
}

void op0_st3_s3(Xtensa &cpu, uint32_t instr)
{
    int t = (instr >> 4) & 0xF;
    switch (t)
    {
        case 0x1:
            cpu.windowed_ret();
            break;
        case 0x3:
            //True NOP
            break;
        default:
            EmuException::die("[Xtensa_Interpreter] Unrecognized op0_st3_s3 $%02X ($%04X)", t, instr);
    }
}

};
