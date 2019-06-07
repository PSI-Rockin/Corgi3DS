#include <sstream>
#include "arm_disasm.hpp"
#include "arm.hpp"
#include "../common/common.hpp"

using namespace std;

ARM_INSTR decode_arm(uint32_t instr)
{
    if ((instr & 0xFE5D0F00) == 0xF84D0500)
        return ARM_SRS;

    if ((instr & 0xFE500F00) == 0xF8100A00)
        return ARM_RFE;

    if (instr == 0xF57FF01F)
        return ARM_CLREX;

    if ((instr & 0x0F000000) == 0x0A000000)
        return ARM_B;

    if ((instr & 0x0F000000) == 0x0B000000)
        return ARM_BL;

    if ((instr & 0x0FFFFFFF) == 0x0320F000)
        return ARM_NOP;

    if ((instr & 0x0FFFFFFF) == 0x0320F001)
        return ARM_YIELD;

    if ((instr & 0x0FFFFFFF) == 0x0320F002)
        return ARM_WFE;

    if ((instr & 0x0FFFFFFF) == 0x0320F003)
        return ARM_WFI;

    if ((instr & 0x0FFFFFFF) == 0x0320F004)
        return ARM_SEV;

    if (((instr >> 4) & 0x0FFFFFF) == 0x12FFF1)
        return ARM_BX;

    if (((instr >> 4) & 0x0FFFFFF) == 0x12FFF3)
        return ARM_BLX;

    if ((instr & 0x0F000000) == 0x0F000000)
        return ARM_SWI;

    if (((instr >> 16) & 0xFFF) == 0x16F)
    {
        if (((instr >> 4) & 0xFF) == 0xF1)
            return ARM_CLZ;
    }

    if (((instr >> 4) & 0xFF) == 0x05)
    {
        if (((instr >> 24) & 0xF) == 0x1)
        {
            int op = (instr >> 20) & 0xF;
            if (op == 0 || op == 2 || op == 4 || op == 6)
                return ARM_SATURATED_OP;
        }
    }

    if ((instr & 0xFFF000F0) == 0xE1200070)
        return ARM_BKPT;

    if ((instr & 0x0FFF00F0) == 0x06AF0070)
        return ARM_SXTB;

    if ((instr & 0x0FFF00F0) == 0x06BF0070)
        return ARM_SXTH;

    if ((instr & 0x0FFF00F0) == 0x06EF0070)
        return ARM_UXTB;

    if ((instr & 0x0FFF00F0) == 0x06FF0070)
        return ARM_UXTH;

    if ((instr & 0xFD70F000) == 0xF550F000)
        return ARM_PLD;

    if ((instr & 0x0FF00FFF) == 0x01D00F9F)
        return ARM_LOAD_EX_BYTE;

    if ((instr & 0x0FF00FF0) == 0x01C00F90)
        return ARM_STORE_EX_BYTE;

    if ((instr & 0x0FF00FFF) == 0x01F00F9F)
        return ARM_LOAD_EX_HALFWORD;

    if ((instr & 0x0FF00FF0) == 0x01E00F90)
        return ARM_STORE_EX_HALFWORD;

    if ((instr & 0x0FF00FFF) == 0x01900F9F)
        return ARM_LOAD_EX_WORD;

    if ((instr & 0x0FF00FF0) == 0x01800F90)
        return ARM_STORE_EX_WORD;

    if ((instr & 0x0FF00FFF) == 0x01B00F9F)
        return ARM_LOAD_EX_DOUBLEWORD;

    if ((instr & 0x0FF00FF0) == 0x01A00F90)
        return ARM_STORE_EX_DOUBLEWORD;

    if (((instr >> 26) & 0x3) == 0)
    {
        if ((instr & (1 << 25)) == 0)
        {
            if (((instr >> 4) & 0xFF) == 0x9)
            {
                if (((instr >> 23) & 0x1F) == 0x2 && (((instr >> 20) & 0x3) == 0))
                    return ARM_SWAP;
            }
            if ((instr & (1 << 7)) != 0 && (instr & (1 << 4)) == 0)
            {
                if ((instr & (1 << 20)) == 0 && ((instr >> 23) & 0x3) == 0x2)
                    return ARM_SIGNED_HALFWORD_MULTIPLY;
            }
            if ((instr & (1 << 7)) != 0 && (instr & (1 << 4)) != 0)
            {
                if (((instr >> 4) & 0xF) == 0x9)
                {
                    if (((instr >> 22) & 0x3F) == 0)
                        return ARM_MULTIPLY;
                    else if (((instr >> 23) & 0x1F) == 1)
                        return ARM_MULTIPLY_LONG;
                    return ARM_UNDEFINED;
                }
                else if ((instr & (1 << 6)) == 0 && ((instr & (1 << 5)) != 0))
                {
                    if ((instr & (1 << 20)) != 0)
                        return ARM_LOAD_HALFWORD;
                    else
                        return ARM_STORE_HALFWORD;
                }
                else if ((instr & (1 << 6)) != 0 && ((instr & (1 << 5))) == 0)
                {
                    if ((instr & (1 << 20)) != 0)
                        return ARM_LOAD_SIGNED_BYTE;
                    else
                        return ARM_LOAD_DOUBLEWORD;
                }
                else if ((instr & (1 << 6)) != 0 && ((instr & (1 << 5))) != 0)
                {
                    if ((instr & (1 << 20)) != 0)
                        return ARM_LOAD_SIGNED_HALFWORD;
                    else
                        return ARM_STORE_DOUBLEWORD;
                }
                return ARM_UNDEFINED;
            }
        }
        return ARM_DATA_PROCESSING;
    }
    else if ((instr & (0x0F000000)) >> 26 == 0x1)
    {
        if ((instr & (1 << 20)) == 0)
        {
            if ((instr & (1 << 22)) == 0)
                return ARM_STORE_WORD;
            else
                return ARM_STORE_BYTE;
        }
        else
        {
            if ((instr & (1 << 22)) == 0)
                return ARM_LOAD_WORD;
            else
                return ARM_LOAD_BYTE;
        }
    }
    else if (((instr >> 25) & 0x7) == 0x4)
    {
        if ((instr & (1 << 20)) == 0)
            return ARM_STORE_BLOCK;
        else
            return ARM_LOAD_BLOCK;
    }
    else if (((instr >> 24) & 0xF) == 0xE)
    {
        if ((instr & (1 << 4)) != 0)
            return ARM_COP_REG_TRANSFER;
        else
            return ARM_COP_DATA_OP;
    }
    return ARM_UNDEFINED;
}

namespace ARM_Disasm
{

string cond_name(int cond)
{
    switch (cond)
    {
        case 0:
            return "eq";
        case 1:
            return "ne";
        case 2:
            return "cs";
        case 3:
            return "cc";
        case 4:
            return "mi";
        case 5:
            return "pl";
        case 6:
            return "vs";
        case 7:
            return "vc";
        case 8:
            return "hi";
        case 9:
            return "ls";
        case 10:
            return "ge";
        case 11:
            return "lt";
        case 12:
            return "gt";
        case 13:
            return "le";
        case 14:
            return "";
        default:
            return "??";
    }
}

string disasm_arm(ARM_CPU& cpu, uint32_t instr)
{
    if ((instr >> 20) == 0xF10)
        return arm_cps(instr);
    switch (decode_arm(instr))
    {
        case ARM_B:
        case ARM_BL:
            return arm_b(cpu, instr);
        case ARM_BX:
        case ARM_BLX:
            return arm_bx(instr);
        case ARM_SWI:
            return arm_swi(cpu, instr);
        case ARM_CLZ:
            return arm_clz(instr);
        case ARM_SXTB:
            return arm_sxtb(instr);
        case ARM_SXTH:
            return arm_sxth(instr);
        case ARM_UXTB:
            return arm_uxtb(instr);
        case ARM_UXTH:
            return arm_uxth(instr);
        case ARM_DATA_PROCESSING:
            return arm_data_processing(instr);
        case ARM_MULTIPLY:
            return arm_mul(instr);
        case ARM_MULTIPLY_LONG:
            return arm_mul_long(instr);
        case ARM_SIGNED_HALFWORD_MULTIPLY:
            return arm_signed_halfword_multiply(instr);
        case ARM_LOAD_WORD:
        case ARM_STORE_WORD:
        case ARM_LOAD_BYTE:
        case ARM_STORE_BYTE:
        case ARM_PLD:
            return arm_load_store(cpu, instr);
        case ARM_LOAD_HALFWORD:
        case ARM_STORE_HALFWORD:
            return arm_load_store_halfword(instr);
        case ARM_LOAD_SIGNED_BYTE:
        case ARM_LOAD_SIGNED_HALFWORD:
            return arm_load_signed(instr);
        case ARM_LOAD_DOUBLEWORD:
        case ARM_STORE_DOUBLEWORD:
            return arm_load_store_doubleword(instr);
        case ARM_LOAD_BLOCK:
        case ARM_STORE_BLOCK:
            return arm_load_store_block(instr);
        case ARM_COP_REG_TRANSFER:
            return arm_cop_transfer(instr);
        case ARM_LOAD_EX_BYTE:
        case ARM_STORE_EX_BYTE:
            return arm_load_store_ex_byte(instr);
        case ARM_LOAD_EX_HALFWORD:
        case ARM_STORE_EX_HALFWORD:
            return arm_load_store_ex_halfword(instr);
        case ARM_LOAD_EX_WORD:
        case ARM_STORE_EX_WORD:
            return arm_load_store_ex_word(instr);
        case ARM_LOAD_EX_DOUBLEWORD:
        case ARM_STORE_EX_DOUBLEWORD:
            return arm_load_store_ex_doubleword(instr);
        case ARM_NOP:
            return "nop {0}";
        case ARM_YIELD:
            return "yield";
        case ARM_WFE:
            return "wfe";
        case ARM_WFI:
            return "wfi";
        case ARM_SEV:
            return "sev";
        case ARM_SRS:
            return arm_srs(instr);
        case ARM_RFE:
            return arm_rfe(instr);
        case ARM_CLREX:
            return "clrex";
        default:
            return "undefined";
    }
}

string arm_cps(uint32_t instr)
{
    stringstream output;
    int psr_mode = instr & 0x1F;
    bool f = (instr >> 6) & 0x1;
    bool i = (instr >> 7) & 0x1;
    bool a = (instr >> 8) & 0x1;
    bool mmod = (instr >> 17) & 0x1;
    int imod = (instr >> 18) & 0x3;

    output << "cps";

    switch (imod)
    {
        case 0x2:
            output << "ie";
            break;
        case 0x3:
            output << "id";
            break;
    }

    output << " ";

    if (a)
        output << "a";
    if (i)
        output << "i";
    if (f)
        output << "f";

    if (mmod)
    {
        if (imod >= 2)
            output << ", ";
        output << "#" << std::hex << "0x" << psr_mode;
    }

    return output.str();
}

string arm_srs(uint32_t instr)
{
    stringstream output;
    bool is_writing_back = instr & (1 << 21);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);

    int mode = instr & 0x1F;

    output << "srs";

    if (is_adding_offset)
        output << "i";
    else
        output << "d";

    if (is_preindexing)
        output << "b";
    else
        output << "a";

    output << " #0x" << std::hex << mode;

    if (is_writing_back)
        output << "!";

    return output.str();
}

string arm_rfe(uint32_t instr)
{
    stringstream output;
    int base = (instr >> 16) & 0xF;
    bool is_writing_back = instr & (1 << 21);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);

    output << "rfe";

    if (is_adding_offset)
        output << "i";
    else
        output << "d";

    if (is_preindexing)
        output << "b";
    else
        output << "a";

    output << " " << ARM_CPU::get_reg_name(base);

    if (is_writing_back)
        output << "!";

    return output.str();
}

string arm_b(ARM_CPU& cpu, uint32_t instr)
{
    stringstream output;
    int condition = instr >> 28;
    int32_t offset = (instr & 0xFFFFFF) << 2;
    offset <<= 6;
    offset >>= 6;
    if (condition == 15)
    {
        output << "blx";
        if (instr & (1 << 24))
            offset += 2;
        output << " $0x" << std::hex << (cpu.get_PC() + offset);
        return output.str();
    }

    output << "b";

    if (instr & (1 << 24))
        output << "l";

    output << cond_name(condition);
    output << " $0x" << std::hex << (cpu.get_PC() + offset);

    return output.str();
}

string arm_bx(uint32_t instr)
{
    string output;
    if (instr & (1 << 5))
        output = "blx";
    else
        output = "bx";
    output += cond_name(instr >> 28);

    output += " ";
    output += ARM_CPU::get_reg_name(instr & 0xF);
    return output;
}

string arm_swi(ARM_CPU &cpu, uint32_t instr)
{
    stringstream output;

    if (cpu.get_id() == 9)
        output << "swi";
    else
        output << "svc";

    output << " 0x" << std::hex << (instr & 0xFFFFFF);

    return output.str();
}

string arm_clz(uint32_t instr)
{
    stringstream output;
    uint32_t source = instr & 0xF;
    uint32_t destination = (instr >> 12) & 0xF;

    output << "clz " << ARM_CPU::get_reg_name(destination) << ", " << ARM_CPU::get_reg_name(source);
    return output.str();
}

string arm_sxtb(uint32_t instr)
{
    stringstream output;
    int source = instr & 0xF;
    int rot = (instr >> 10) & 0x3;
    int dest = (instr >> 12) & 0xF;

    output << "sxtb " << ARM_CPU::get_reg_name(dest) << ", " << ARM_CPU::get_reg_name(source);

    if (rot)
        output << ", ror #" << std::dec << rot * 8;

    return output.str();
}

string arm_sxth(uint32_t instr)
{
    stringstream output;
    int source = instr & 0xF;
    int rot = (instr >> 10) & 0x3;
    int dest = (instr >> 12) & 0xF;

    output << "sxth " << ARM_CPU::get_reg_name(dest) << ", " << ARM_CPU::get_reg_name(source);

    if (rot)
        output << ", ror #" << std::dec << rot * 8;

    return output.str();
}

string arm_uxtb(uint32_t instr)
{
    stringstream output;
    int source = instr & 0xF;
    int rot = (instr >> 10) & 0x3;
    int dest = (instr >> 12) & 0xF;

    output << "uxtb " << ARM_CPU::get_reg_name(dest) << ", " << ARM_CPU::get_reg_name(source);

    if (rot)
        output << ", ror #" << std::dec << rot * 8;

    return output.str();
}

string arm_uxth(uint32_t instr)
{
    stringstream output;
    int source = instr & 0xF;
    int rot = (instr >> 10) & 0x3;
    int dest = (instr >> 12) & 0xF;

    output << "uxth " << ARM_CPU::get_reg_name(dest) << ", " << ARM_CPU::get_reg_name(source);

    if (rot)
        output << ", ror #" << std::dec << rot * 8;

    return output.str();
}

string arm_data_processing(uint32_t instr)
{
    stringstream output;
    int opcode = (instr >> 21) & 0xF;
    int first_operand = (instr >> 16) & 0xF;
    int destination = (instr >> 12) & 0xF;
    bool set_condition_codes = instr & (1 << 20);
    bool is_imm = instr & (1 << 25);
    int second_operand;
    stringstream second_operand_str;
    int shift = 0;

    if (is_imm)
    {
        second_operand = instr & 0xFF;
        shift = (instr & 0xF00) >> 7;

        second_operand = rotr32(second_operand, shift);
        second_operand_str << "#0x" << std::hex << second_operand;
    }
    else
    {
        stringstream shift_str;
        second_operand = instr & 0xF;
        second_operand_str << ARM_CPU::get_reg_name(second_operand);
        int shift_type = (instr >> 5) & 0x3;
        bool imm_shift = (instr & (1 << 4)) == 0;
        shift = (instr >> 7) & 0x1F;

        switch (shift_type)
        {
            case 0:
                if (!imm_shift || shift)
                    second_operand_str << ", lsl";
                break;
            case 1:
                second_operand_str << ", lsr";
                if (!shift)
                    shift = 32;
                break;
            case 2:
                second_operand_str << ", asr";
                if (!shift)
                    shift = 32;
                break;
            case 3:
                if (imm_shift && !shift)
                    second_operand_str << ", rrx";
                else
                    second_operand_str << ", ror";
                break;
        }

        if (!imm_shift)
        {
            second_operand_str << " " << ARM_CPU::get_reg_name((instr >> 8) & 0xF);
        }
        else
        {
            if (shift || shift_type == 1 || shift_type == 2)
                second_operand_str << " #" << ((instr >> 7) & 0x1F);
        }
    }

    bool use_first_op = true;
    bool use_dest = true;

    switch (opcode)
    {
        case 0x0:
            output << "and";
            break;
        case 0x1:
            output << "eor";
            break;
        case 0x2:
            output << "sub";
            break;
        case 0x3:
            output << "rsb";
            break;
        case 0x4:
            output << "add";
            break;
        case 0x5:
            output << "adc";
            break;
        case 0x6:
            output << "sbc";
            break;
        case 0x7:
            output << "rsc";
            break;
        case 0x8:
            if (set_condition_codes)
            {
                output << "tst";
                use_dest = false;
                set_condition_codes = false;
            }
            else
                return arm_mrs(instr);
            break;
        case 0x9:
            if (set_condition_codes)
            {
                output << "teq";
                use_dest = false;
                set_condition_codes = false;
            }
            else
                return arm_msr(instr);
            break;
        case 0xA:
            if (set_condition_codes)
            {
                output << "cmp";
                use_dest = false;
                set_condition_codes = false;
            }
            else
                return arm_mrs(instr);
            break;
        case 0xB:
            if (set_condition_codes)
            {
                output << "cmn";
                use_dest = false;
                set_condition_codes = false;
            }
            else
                return arm_msr(instr);
            break;
        case 0xC:
            output << "orr";
            break;
        case 0xD:
            output << "mov";
            use_first_op = false;
            break;
        case 0xE:
            output << "bic";
            break;
        case 0xF:
            output << "mvn";
            use_first_op = false;
            break;
    }

    if (set_condition_codes)
        output << "s";

    output << cond_name(instr >> 28) << " ";

    if (use_dest)
        output << ARM_CPU::get_reg_name(destination) << ", ";

    if (use_first_op)
        output << ARM_CPU::get_reg_name(first_operand) << ", ";

    output << second_operand_str.str();
    return output.str();
}

string arm_signed_halfword_multiply(uint32_t instr)
{
    stringstream output;
    uint32_t destination = (instr >> 16) & 0xF;
    uint32_t accumulate = (instr >> 12) & 0xF;
    uint32_t first_operand = (instr >> 8) & 0xF;
    uint32_t second_operand = instr & 0xF;
    int opcode = (instr >> 21) & 0xF;

    bool first_op_top = instr & (1 << 6);
    bool second_op_top = instr & (1 << 5);

    string first_op_str;
    string second_op_str;

    if (first_op_top)
        first_op_str = "t";
    else
        first_op_str = "b";

    if (second_op_top)
        second_op_str = "t";
    else
        second_op_str = "b";

    switch (opcode)
    {
        case 0xB:
            output << "smul" << first_op_str << second_op_str << cond_name(instr >> 28);
            output << " " << ARM_CPU::get_reg_name(destination) << ", " << ARM_CPU::get_reg_name(second_operand);
            output << ", " << ARM_CPU::get_reg_name(first_operand);
            break;
        default:
            return "undefined signed halfword";
    }
    return output.str();
}

string arm_mul(uint32_t instr)
{
    stringstream output;
    bool accumulate = instr & (1 << 21);
    bool set_condition_codes = instr & (1 << 20);
    int destination = (instr >> 16) & 0xF;
    int first_operand = instr & 0xF;
    int second_operand = (instr >> 8) & 0xF;
    int third_operand = (instr >> 12) & 0xF;

    if (accumulate)
        output << "mla";
    else
        output << "mul";
    if (set_condition_codes)
        output << "s";

    output << cond_name(instr >> 28);

    output << " " << ARM_CPU::get_reg_name(destination) << ", " << ARM_CPU::get_reg_name(first_operand);
    output << ", " << ARM_CPU::get_reg_name(second_operand);
    if (accumulate)
        output << ", " << ARM_CPU::get_reg_name(third_operand);
    return output.str();
}

string arm_mul_long(uint32_t instr)
{
    stringstream output;
    bool is_signed = instr & (1 << 22);
    bool accumulate = instr & (1 << 21);
    bool set_condition_codes = instr & (1 << 20);

    int dest_hi = (instr >> 16) & 0xF;
    int dest_lo = (instr >> 12) & 0xF;
    uint32_t first_operand = (instr >> 8) & 0xF;
    uint32_t second_operand = (instr) & 0xF;

    if (is_signed)
        output << "s";
    else
        output << "u";

    if (accumulate)
        output << "mlal";
    else
        output << "mull";
    if (set_condition_codes)
        output << "s";

    output << cond_name(instr >> 28);
    output << " " << ARM_CPU::get_reg_name(dest_lo) << ", " << ARM_CPU::get_reg_name(dest_hi);
    output << ", " << ARM_CPU::get_reg_name(second_operand) << ", " << ARM_CPU::get_reg_name(first_operand);

    return output.str();
}

string arm_mrs(uint32_t instr)
{
    stringstream output;
    output << "mrs";
    output << cond_name(instr >> 28) << " ";
    output << ARM_CPU::get_reg_name((instr >> 12) & 0xF) << ", ";
    bool using_CPSR = (instr & (1 << 22)) == 0;
    if (using_CPSR)
        output << "CPSR";
    else
        output << "SPSR";
    return output.str();
}

string arm_msr(uint32_t instr)
{
    stringstream output;
    output << "msr";
    output << cond_name(instr >> 28) << " ";

    bool using_CPSR = (instr & (1 << 22)) == 0;
    if (using_CPSR)
        output << "CPSR";
    else
        output << "SPSR";

    output << ", ";

    bool is_imm = instr & (1 << 25);
    if (is_imm)
    {
        int operand = instr & 0xFF;
        int shift = (instr & 0xFF0) >> 7;
        operand = rotr32(operand, shift);
        output << "#0x" << std::hex << operand;
    }
    else
        output << ARM_CPU::get_reg_name(instr & 0xF);
    return output.str();
}

string arm_load_store(ARM_CPU& cpu, uint32_t instr)
{
    stringstream output;
    bool is_loading = instr & (1 << 20);
    bool is_pld = ((instr >> 28) & 0xF) == 0xF;
    if (is_pld)
        output << "pld";
    else
    {
        if (is_loading)
            output << "ldr";
        else
            output << "str";
        if (instr & (1 << 22))
            output << "b";
        output << cond_name(instr >> 28);
    }

    bool is_adding_offset = instr & (1 << 23);
    bool pre_indexing = instr & (1 << 24);
    int reg = (instr >> 12) & 0xF;
    int base = (instr >> 16) & 0xF;
    bool is_imm = (instr & (1 << 25)) == 0;
    stringstream offset_str;

    //Handle ldr =adr
    if (is_loading && base == REG_PC && is_imm)
    {
        uint32_t literal = cpu.get_PC();
        if (is_adding_offset)
            literal += instr & 0xFFF;
        else
            literal -= instr & 0xFFF;
        output << " " << ARM_CPU::get_reg_name(reg) << ", =0x";
        output << std::hex << cpu.read32(literal);
        return output.str();
    }
    string sub_str;
    if (!is_adding_offset)
        sub_str = "-";
    if (is_imm)
    {
        int offset = instr & 0xFFF;
        if (offset)
            offset_str << ", " << sub_str << "#0x" << std::hex << offset;
    }
    else
    {
        offset_str << ", " << sub_str << ARM_CPU::get_reg_name(instr & 0xF);

        int shift_type = (instr >> 5) & 0x3;
        int shift = (instr >> 7) & 0x1F;

        switch (shift_type)
        {
            case 0:
                if (shift)
                    offset_str << ", lsl #" << shift;
                break;
            case 1:
                offset_str << ", lsr #";
                if (!shift)
                    shift = 32;
                offset_str << shift;
                break;
            case 2:
                offset_str << ", asr #";
                if (!shift)
                    shift = 32;
                offset_str << shift;
                break;
            case 3:
                if (!shift)
                    offset_str << ", rrx";
                else
                    offset_str << ", ror #" << shift;
                break;
        }
    }


    if (!is_pld)
        output << " " << ARM_CPU::get_reg_name(reg) << ", ";
    else
        output << " ";
    output << "[" << ARM_CPU::get_reg_name(base);

    if (pre_indexing)
        output << offset_str.str();
    output << "]";
    if (pre_indexing && (instr & (1 << 21)))
        output << "!";
    if (!pre_indexing)
        output << offset_str.str();
    return output.str();
}

string arm_load_store_halfword(uint32_t instr)
{
    stringstream output;
    bool is_preindexing = (instr & (1 << 24)) != 0;
    bool is_adding_offset = (instr & (1 << 23)) != 0;
    bool is_imm_offset = (instr & (1 << 22)) != 0;
    bool is_writing_back = (instr & (1 << 21)) != 0;
    bool is_loading = (instr & (1 << 20)) != 0;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t reg = (instr >> 12) & 0xF;

    if (is_loading)
        output << "ldr";
    else
        output << "str";

    output << cond_name(instr >> 28);

    output << "h " << ARM_CPU::get_reg_name(reg) << ", [" << ARM_CPU::get_reg_name(base);

    int offset = ((instr >> 4) & 0xF0) | (instr & 0xF);
    stringstream offset_stream;
    string sub_str = "";
    if (!is_adding_offset)
        sub_str = "-";
    if (is_imm_offset)
    {
        if (offset)
            offset_stream << sub_str << "#" << std::hex << offset;
    }
    else
        offset_stream << sub_str << ARM_CPU::get_reg_name(offset & 0xF);

    string offset_str = offset_stream.str();
    if (is_preindexing)
    {
        if (offset_str.length())
            output << ", " << offset_str;
        output << "]";
        if (is_writing_back)
            output << "!";
    }
    else
    {
        output << "]";
        if (offset_str.length())
            output << ", " << offset_str;
    }

    return output.str();
}

string arm_load_signed(uint32_t instr)
{
    stringstream output;
    bool is_preindexing = (instr & (1 << 24)) != 0;
    bool is_adding_offset = (instr & (1 << 23)) != 0;
    bool is_writing_back = (instr & (1 << 21)) != 0;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t destination = (instr >> 12) & 0xF;

    bool is_imm_offset = (instr & (1 << 22)) != 0;
    bool load_halfword = (instr & (1 << 5)) != 0;

    output << "ldrs";
    if (load_halfword)
        output << "h";
    else
        output << "b";

    output << cond_name(instr >> 28);
    output << " " << ARM_CPU::get_reg_name(destination) << ", [";
    output << ARM_CPU::get_reg_name(base);

    stringstream offset_str;
    string sub_str;
    if (!is_adding_offset)
        sub_str = "-";
    if (is_imm_offset)
    {
        int offset = ((instr >> 4) & 0xF0) | (instr & 0xF);
        if (offset)
            offset_str << sub_str << ", #0x" << std::hex << offset;
    }
    else
    {
        offset_str << sub_str << ", " << ARM_CPU::get_reg_name(instr & 0xF);
    }
    if (is_preindexing)
        output << offset_str.str();
    output << "]";
    if (is_writing_back && is_preindexing)
        output << "!";
    if (!is_preindexing)
        output << offset_str.str();
    return output.str();
}

string arm_load_store_doubleword(uint32_t instr)
{
    stringstream output;
    bool pre_indexing = instr & (1 << 24);
    bool add_offset = instr & (1 << 23);
    bool is_imm_offset = instr & (1 << 22);

    uint32_t base = (instr >> 16) & 0xF;
    uint32_t reg = (instr >> 12) & 0xF;
    bool load = !(instr & (1 << 5));

    int offset = instr & 0xF;
    string sub_str;

    if (load)
        output << "ldrd ";
    else
        output << "strd ";

    if (!add_offset)
        sub_str = "-";

    output << ARM_CPU::get_reg_name(reg) << ", [";
    output << ARM_CPU::get_reg_name(base);

    stringstream offset_str;
    if (is_imm_offset)
    {
        offset |= (instr >> 4) & 0xF0;
        if (offset)
            offset_str << ", #" << sub_str << std::hex << offset;
    }
    else
        offset_str << ", " << sub_str << ARM_CPU::get_reg_name(offset);

    if (pre_indexing)
        output << offset_str.str();
    output << "]";
    if (pre_indexing && (instr & (1 << 21)))
        output << "!";
    if (!pre_indexing)
        output << offset_str.str();

    return output.str();
}

string arm_load_store_ex_byte(uint32_t instr)
{
    stringstream output;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t reg = (instr >> 12) & 0xF;
    uint32_t reg2 = instr & 0xF;
    bool load = (instr & (1 << 20)) != 0;

    if (load)
        output << "ldrexb";
    else
        output << "strexb";

    output << cond_name(instr >> 28) << " ";
    output << ARM_CPU::get_reg_name(reg) << ", ";

    if (!load)
        output << ARM_CPU::get_reg_name(reg2) << ", ";
    output << "[" << ARM_CPU::get_reg_name(base) << "]";

    return output.str();
}

string arm_load_store_ex_halfword(uint32_t instr)
{
    stringstream output;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t reg = (instr >> 12) & 0xF;
    uint32_t reg2 = instr & 0xF;
    bool load = (instr & (1 << 20)) != 0;

    if (load)
        output << "ldrexh";
    else
        output << "strexh";

    output << cond_name(instr >> 28) << " ";
    output << ARM_CPU::get_reg_name(reg) << ", ";

    if (!load)
        output << ARM_CPU::get_reg_name(reg2) << ", ";
    output << "[" << ARM_CPU::get_reg_name(base) << "]";

    return output.str();
}

string arm_load_store_ex_word(uint32_t instr)
{
    stringstream output;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t reg = (instr >> 12) & 0xF;
    uint32_t reg2 = instr & 0xF;
    bool load = (instr & (1 << 20)) != 0;

    if (load)
        output << "ldrex";
    else
        output << "strex";

    output << cond_name(instr >> 28) << " ";
    output << ARM_CPU::get_reg_name(reg) << ", ";

    if (!load)
        output << ARM_CPU::get_reg_name(reg2) << ", ";
    output << "[" << ARM_CPU::get_reg_name(base) << "]";

    return output.str();
}

string arm_load_store_ex_doubleword(uint32_t instr)
{
    stringstream output;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t reg = (instr >> 12) & 0xF;
    uint32_t reg2 = instr & 0xF;
    bool load = (instr & (1 << 20)) != 0;

    if (load)
        output << "ldrexd";
    else
        output << "strexd";

    output << cond_name(instr >> 28) << " ";
    output << ARM_CPU::get_reg_name(reg) << ", ";

    if (!load)
        output << ARM_CPU::get_reg_name(reg2) << ", ";
    output << "[" << ARM_CPU::get_reg_name(base) << "]";

    return output.str();
}

string arm_load_store_block(uint32_t instr)
{
    stringstream output;
    uint16_t reg_list = instr & 0xFFFF;
    uint32_t base = (instr >> 16) & 0xF;
    bool load_block = instr & (1 << 20);
    bool is_writing_back = instr & (1 << 21);
    bool load_PSR = instr & (1 << 22);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);

    if (!load_block && base == REG_SP && !is_adding_offset && is_preindexing && is_writing_back)
        output << "push" << cond_name(instr >> 28);
    else if (load_block && base == REG_SP && is_adding_offset && !is_preindexing && is_writing_back)
        output << "pop" << cond_name(instr >> 28);
    else
    {
        if (load_block)
            output << "ldm";
        else
            output << "stm";
        if (is_adding_offset)
            output << "i";
        else
            output << "d";

        if (is_preindexing)
            output << "b";
        else
            output << "a";

        output << cond_name(instr >> 28);

        output << " " << ARM_CPU::get_reg_name(base);
        if (is_writing_back)
            output << "!";

        output << ",";
    }

    output << " {";
    bool on_first_reg = true;
    int consecutive_regs = 0;

    for (int i = 0; i <= 16; i++)
    {
        if (reg_list & (1 << i))
            consecutive_regs++;
        else
        {
            if (consecutive_regs)
            {
                if (!on_first_reg)
                    output << ", ";
                output << ARM_CPU::get_reg_name(i - consecutive_regs);
                if (consecutive_regs > 1)
                    output << "-" << ARM_CPU::get_reg_name(i - 1);
                on_first_reg = false;
            }
            consecutive_regs = 0;
        }
    }
    output << "}";

    if (load_PSR)
        output << "^";
    return output.str();
}

string arm_cop_transfer(uint32_t instr)
{
    stringstream output;
    int operation_mode = (instr >> 21) & 0x7;
    bool is_loading = (instr & (1 << 20)) != 0;
    uint32_t CP_reg = (instr >> 16) & 0xF;
    uint32_t ARM_reg = (instr >> 12) & 0xF;

    int coprocessor_id = (instr >> 8) & 0xF;
    int coprocessor_info = (instr >> 5) & 0x7;
    int coprocessor_operand = instr & 0xF;

    if (is_loading)
        output << "mrc";
    else
        output << "mcr";

    output << " " << coprocessor_id << ", " << operation_mode << ", " << ARM_CPU::get_reg_name(ARM_reg);
    output << ", cr" << CP_reg << ", cr" << coprocessor_operand << ", {" << coprocessor_info << "}";
    return output.str();
}

};
