#include <sstream>
#include "arm.hpp"
#include "arm_disasm.hpp"

using namespace std;

namespace ARM_Disasm
{

string vfp_single_transfer(uint32_t instr)
{
    uint8_t op = (instr >> 21) & 0x7;
    bool op_hi = (instr >> 8) & 0x1;

    op |= op_hi << 4;

    switch (op)
    {
        case 0x00:
            return vfp_mov_fpr_gpr(instr);
        case 0x07:
            return vfp_mov_sys_reg(instr);
        default:
            return "undefined";
    }
}

string vfp_mov_fpr_gpr(uint32_t instr)
{
    stringstream output;
    bool load_to_gpr = (instr >> 20) & 0x1;
    int gpr = (instr >> 12) & 0xF;
    int fpr = (instr >> 16) & 0xF;
    bool fpr_hi = (instr >> 7) & 0x1;

    fpr <<= 1;
    fpr |= fpr_hi;

    if (load_to_gpr)
        output << "fmrs " << ARM_CPU::get_reg_name(gpr) << ", s" << dec << fpr;
    else
        output << "fmsr s" << dec << fpr << ", " << ARM_CPU::get_reg_name(gpr);

    return output.str();
}

string vfp_mov_sys_reg(uint32_t instr)
{
    stringstream output;

    bool load_to_gpr = (instr >> 20) & 0x1;
    int gpr = (instr >> 12) & 0xF;
    int sys = (instr >> 16) & 0xF;
    bool sys_hi = (instr >> 7) & 0x1;

    if (sys_hi)
        return "undefined";

    string sys_reg;
    switch (sys)
    {
        case 0x0:
            sys_reg = "fpsid";
            break;
        case 0x1:
            if (gpr == 15)
                return "fmstat";
            sys_reg = "fpscr";
            break;
        case 0x8:
            sys_reg = "fpexc";
            break;
    }

    if (load_to_gpr)
        output << "fmrx " << ARM_CPU::get_reg_name(gpr) << ", " << sys_reg;
    else
        output << "fmxr " << sys_reg << ", " << ARM_CPU::get_reg_name(gpr);

    return output.str();
}

string vfp_load_store(uint32_t instr)
{
    bool p = (instr >> 24) & 0x1;
    bool u = (instr >> 23) & 0x1;
    bool w = (instr >> 21) & 0x1;
    bool l = (instr >> 20) & 0x1;

    uint16_t op = (p << 2) | (u << 1) | w;

    switch (op)
    {
        case 0x02:
        case 0x03:
        case 0x05:
            return vfp_load_store_block(instr);
        case 0x04:
        case 0x06:
            return vfp_load_store_single(instr);
        default:
        {
            stringstream und_output;
            und_output << "undefined vfp load/store 0x" << hex << op;
            return und_output.str();
        }
    }
}

string vfp_load_store_block(uint32_t instr)
{
    stringstream output;

    bool p = (instr >> 24) & 0x1;
    bool u = (instr >> 23) & 0x1;
    bool d = (instr >> 22) & 0x1;
    bool w = (instr >> 21) & 0x1;
    bool l = (instr >> 20) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t start = (instr >> 12) & 0xF;
    bool is_double = (instr >> 8) & 0x1;
    uint16_t offset = instr & 0xFF;
    if (is_double)
        offset >>= 1;

    if (l)
        output << "fldm";
    else
        output << "fstm";

    if (u)
        output << "ia";
    else
        output << "db";

    string precision;

    if (is_double)
        precision = "d";
    else
    {
        precision = "s";
        start <<= 1;
        start |= d;
    }

    output << precision;

    output << cond_name(instr >> 28) << " ";

    output << ARM_CPU::get_reg_name(base);
    if (w)
        output << "!";

    output << ", {";

    if ((instr >> 22) & 0x1)
        start += 16;

    uint32_t end = start + offset - 1;
    output << precision << dec << start;
    if (end > start)
        output << "-" << precision << end;
    output << "}";

    return output.str();
}

string vfp_load_store_single(uint32_t instr)
{
    stringstream output;

    bool p = (instr >> 24) & 0x1;
    bool u = (instr >> 23) & 0x1;
    bool d = (instr >> 22) & 0x1;
    bool l = (instr >> 20) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t fp_reg = (instr >> 12) & 0xF;
    bool is_double = (instr >> 8) & 0x1;
    uint16_t offset = instr & 0xFF;
    offset <<= 2;

    if (l)
        output << "fld";
    else
        output << "fst";

    string precision;

    if (is_double)
        precision = "d";
    else
    {
        precision = "s";
        fp_reg <<= 1;
        fp_reg |= d;
    }

    output << precision;

    output << cond_name(instr >> 28) << " ";

    output << precision << dec << fp_reg << ", [" << ARM_CPU::get_reg_name(base);

    if (offset)
    {
        output << ",";
        if (!u)
            output << "-";

        output << " #0x" << hex << offset;
    }

    output << "]";

    return output.str();
}

string vfp_data_processing(uint32_t instr)
{
    bool p = (instr >> 23) & 0x1;
    bool q = (instr >> 21) & 0x1;
    bool r = (instr >> 20) & 0x1;
    bool s = (instr >> 6) & 0x1;

    uint8_t op = (p << 3) | (q << 2) | (r << 1) | s;

    if (op == 0xF)
        return vfp_data_extended(instr);
    else if (op > 0x8)
        return "undefined";

    const static char* ops[] = {"fmac", "fnmac", "fmsc", "fnmsc", "fmul", "fnmul", "fadd", "fsub", "fdiv"};

    int dest = (instr >> 12) & 0xF;
    int reg1 = (instr >> 16) & 0xF;
    int reg2 = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool reg1_low_bit = (instr >> 7) & 0x1;
    bool reg2_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    stringstream output;

    string precision;
    if (is_double)
    {
        precision = "d";
    }
    else
    {
        dest <<= 1;
        reg1 <<= 1;
        reg2 <<= 1;
        dest |= dest_low_bit;
        reg1 |= reg1_low_bit;
        reg2 |= reg2_low_bit;
        precision = "s";
    }

    output << ops[op] << precision;
    output << cond_name(instr >> 28) << " ";

    output << precision << dec << dest << ", " << precision << reg1 << ", " << precision << reg2;

    return output.str();
}

string vfp_data_extended(uint32_t instr)
{
    stringstream output;

    uint8_t op = (instr >> 16) & 0xF;
    bool op_hi = (instr >> 7) & 0x1;
    op |= op_hi << 4;

    switch (op)
    {
        case 0x00:
            output << "fcpy";
            break;
        case 0x01:
            output << "fneg";
            break;
        case 0x08:
            output << "fuito";
            break;
        case 0x0C:
            output << "ftoui";
            break;
        case 0x10:
            output << "fabs";
            break;
        case 0x11:
            output << "fsqrt";
            break;
        case 0x14:
            output << "fcmpe";
            break;
        case 0x18:
            output << "fsito";
            break;
        case 0x1C:
            output << "ftouiz";
            break;
        case 0x1D:
            output << "ftosiz";
            break;
        default:
            return "undefined";
    }

    int dest = (instr >> 12) & 0xF;
    int source = instr & 0xF;
    bool dest_low_bit = (instr >> 22) & 0x1;
    bool source_low_bit = (instr >> 5) & 0x1;
    bool is_double = (instr >> 8) & 0x1;

    string precision;
    if (is_double)
    {
        precision = "d";
    }
    else
    {
        dest <<= 1;
        source <<= 1;
        dest |= dest_low_bit;
        source |= source_low_bit;
        precision = "s";
    }

    output << precision << cond_name(instr >> 28) << " ";

    output << precision << dec << dest << ", " << precision << source;

    return output.str();
}

};
