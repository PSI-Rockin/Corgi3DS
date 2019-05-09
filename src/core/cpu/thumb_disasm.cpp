#include <sstream>
#include "arm.hpp"
#include "arm_disasm.hpp"

using namespace std;

THUMB_INSTR decode_thumb(uint16_t instr)
{
    uint16_t instr13 = instr >> 13;
    uint16_t instr12 = instr >> 12;
    uint16_t instr11 = instr >> 11;
    uint16_t instr10 = instr >> 10;
    switch (instr11)
    {
        case 0x4:
            return THUMB_MOV_IMM;
        case 0x5:
            return THUMB_CMP_IMM;
        case 0x6:
            return THUMB_ADD_IMM;
        case 0x7:
            return THUMB_SUB_IMM;
        case 0x9:
            return THUMB_PC_REL_LOAD;
        case 0x10:
            return THUMB_STORE_HALFWORD;
        case 0x11:
            return THUMB_LOAD_HALFWORD;
        case 0x12:
            return THUMB_SP_REL_STORE;
        case 0x13:
            return THUMB_SP_REL_LOAD;
        case 0x18:
            return THUMB_STORE_MULTIPLE;
        case 0x19:
            return THUMB_LOAD_MULTIPLE;
        case 0x1C:
            return THUMB_BRANCH;
        case 0x1D:
            return THUMB_LONG_BLX;
    }
    if (instr13 == 0)
    {
        if ((instr11 & 0x3) != 0x3)
            return THUMB_MOV_SHIFT;
        else
        {
            if ((instr & (1 << 9)) != 0)
                return THUMB_SUB_REG;
            return THUMB_ADD_REG;
        }
    }
    if (instr10 == 0x10)
        return THUMB_ALU_OP;
    if (instr10 == 0x11)
        return THUMB_HI_REG_OP;
    if (instr12 == 0x5)
    {
        if ((instr & (1 << 9)) == 0)
        {
            if ((instr & (1 << 11)) == 0)
                return THUMB_STORE_REG_OFFSET;
            return THUMB_LOAD_REG_OFFSET;
        }
        return THUMB_LOAD_STORE_SIGN_HALFWORD;
    }
    if (instr13 == 0x3)
    {
        if ((instr & (1 << 11)) == 0)
            return THUMB_STORE_IMM_OFFSET;
        return THUMB_LOAD_IMM_OFFSET;
    }
    if (instr12 == 0xA)
        return THUMB_LOAD_ADDRESS;
    if (instr12 == 0xB)
    {
        if (((instr >> 9) & 0x3) == 0x2)
        {
            if ((instr & (1 << 11)) != 0)
                return THUMB_POP;
            return THUMB_PUSH;
        }
        if (instr & (1 << 9))
        {
            int op = (instr >> 6) & 0x3;
            switch (op)
            {
                case 0:
                    return THUMB_SXTH;
                case 1:
                    return THUMB_SXTB;
                case 2:
                    return THUMB_UXTH;
                case 3:
                    return THUMB_UXTB;
            }
        }
        return THUMB_OFFSET_SP;
    }
    if (instr12 == 0xD)
        return THUMB_COND_BRANCH;
    if (instr12 == 0xF)
    {
        if ((instr & (1 << 11)) == 0)
            return THUMB_LONG_BRANCH_PREP;
        return THUMB_LONG_BRANCH;
    }
    return THUMB_UNDEFINED;
}

namespace ARM_Disasm
{

string disasm_thumb(ARM_CPU& cpu, uint16_t instr)
{
    switch (decode_thumb(instr))
    {
        case THUMB_MOV_SHIFT:
            return thumb_move_shift(instr);
        case THUMB_ADD_REG:
        case THUMB_SUB_REG:
            return thumb_op_reg(instr);
        case THUMB_ADD_IMM:
        case THUMB_CMP_IMM:
        case THUMB_MOV_IMM:
        case THUMB_SUB_IMM:
            return thumb_op_imm(instr);
        case THUMB_ALU_OP:
            return thumb_alu(instr);
        case THUMB_HI_REG_OP:
            return thumb_hi_reg_op(instr);
        case THUMB_LOAD_IMM_OFFSET:
        case THUMB_STORE_IMM_OFFSET:
            return thumb_load_store_imm(instr);
        case THUMB_LOAD_REG_OFFSET:
        case THUMB_STORE_REG_OFFSET:
            return thumb_load_store_reg(instr);
        case THUMB_LOAD_HALFWORD:
        case THUMB_STORE_HALFWORD:
            return thumb_load_store_halfword(instr);
        case THUMB_LOAD_STORE_SIGN_HALFWORD:
            return thumb_load_store_sign_halfword(instr);
        case THUMB_LOAD_MULTIPLE:
        case THUMB_STORE_MULTIPLE:
            return thumb_load_store_block(instr);
        case THUMB_PUSH:
        case THUMB_POP:
            return thumb_push_pop(instr);
        case THUMB_PC_REL_LOAD:
            return thumb_pc_rel_load(cpu, instr);
        case THUMB_LOAD_ADDRESS:
            return thumb_load_addr(cpu, instr);
        case THUMB_SP_REL_LOAD:
        case THUMB_SP_REL_STORE:
            return thumb_sp_rel_load_store(instr);
        case THUMB_OFFSET_SP:
            return thumb_offset_sp(instr);
        case THUMB_SXTH:
        case THUMB_SXTB:
        case THUMB_UXTH:
        case THUMB_UXTB:
            return thumb_extend_op(instr);
        case THUMB_BRANCH:
            return thumb_branch(cpu, instr);
        case THUMB_COND_BRANCH:
            return thumb_cond_branch(cpu, instr);
        case THUMB_LONG_BRANCH_PREP:
            return thumb_long_branch(cpu, instr);
        case THUMB_LONG_BRANCH:
        case THUMB_LONG_BLX:
            return "..";
        default:
            return "undefined";
    }
}

string thumb_move_shift(uint16_t instr)
{
    stringstream output;
    int opcode = (instr >> 11) & 0x3;
    int shift = (instr >> 6) & 0x1F;
    uint32_t source = (instr >> 3) & 0x7;
    uint32_t destination = instr & 0x7;

    switch (opcode)
    {
        case 0:
            if (shift)
                output << "lsls";
            else
                output << "movs";
            break;
        case 1:
            if (!shift)
                shift = 32;
            output << "lsrs";
            break;
        case 2:
            if (!shift)
                shift = 32;
            output << "asrs";
            break;
        default:
            return "UNDEFINED";
    }

    output << " " << ARM_CPU::get_reg_name(destination) << ", " << ARM_CPU::get_reg_name(source);
    if (shift)
        output << ", #" << shift;

    return output.str();
}

string thumb_op_imm(uint16_t instr)
{
    stringstream output;
    const static string ops[] = {"movs", "cmp", "adds", "subs"};
    int opcode = (instr >> 11) & 0x3;
    int dest = (instr >> 8) & 0x7;
    int offset = instr & 0xFF;

    output << ops[opcode] << " " << ARM_CPU::get_reg_name(dest) << ", #0x" << std::hex << offset;
    return output.str();
}

string thumb_op_reg(uint16_t instr)
{
    stringstream output;
    uint32_t destination = instr & 0x7;
    uint32_t source = (instr >> 3) & 0x7;
    uint32_t operand = (instr >> 6) & 0x7;
    bool sub = instr & (1 << 9);
    bool is_imm = instr & (1 << 10);

    if (!sub)
        output << "adds";
    else
        output << "subs";

    output << " " << ARM_CPU::get_reg_name(destination) << ", " << ARM_CPU::get_reg_name(source) << ", ";
    if (is_imm)
        output << "#0x" << std::hex << operand;
    else
        output << ARM_CPU::get_reg_name(operand);
    return output.str();
}

string thumb_alu(uint16_t instr)
{
    const static string ops[] =
        {"ands", "eors", "lsls", "lsrs", "asrs", "adcs", "sbcs", "rors",
         "tst", "negs", "cmp", "cmn", "orrs", "muls", "bics", "mvns"};
    stringstream output;
    uint32_t destination = instr & 0x7;
    uint32_t source = (instr >> 3) & 0x7;
    int opcode = (instr >> 6) & 0xF;

    output << ops[opcode] << " " << ARM_CPU::get_reg_name(destination) << ", " << ARM_CPU::get_reg_name(source);
    return output.str();
}

string thumb_hi_reg_op(uint16_t instr)
{
    stringstream output;
    int opcode = (instr >> 8) & 0x3;
    bool high_source = (instr & (1 << 6)) != 0;
    bool high_dest = (instr & (1 << 7)) != 0;

    uint32_t source = (instr >> 3) & 0x7;
    source |= high_source * 8;
    uint32_t destination = instr & 0x7;
    destination |= high_dest * 8;

    bool use_dest = true;

    switch (opcode)
    {
        case 0x0:
            output << "add";
            break;
        case 0x1:
            output << "cmp";
            break;
        case 0x2:
            output << "mov";
            break;
        case 0x3:
            if (high_dest)
                output << "blx";
            else
                output << "bx";
            use_dest = false;
            break;
    }

    output << " ";

    if (use_dest)
        output << ARM_CPU::get_reg_name(destination) << ", ";
    output << ARM_CPU::get_reg_name(source);
    return output.str();
}

string thumb_load_store_imm(uint16_t instr)
{
    stringstream output;
    bool is_loading = instr & (1 << 11);
    bool is_byte = (instr & (1 << 12));

    uint32_t base = (instr >> 3) & 0x7;
    uint32_t source_dest = instr & 0x7;
    uint32_t offset = (instr >> 6) & 0x1F;

    if (is_loading)
        output << "ldr";
    else
        output << "str";

    if (is_byte)
        output << "b";
    else
        offset <<= 2;

    output << " " << ARM_CPU::get_reg_name(source_dest) << ", [" << ARM_CPU::get_reg_name(base);
    if (offset)
        output << ", #0x" << std::hex << offset;
    output << "]";

    return output.str();
}

string thumb_load_store_reg(uint16_t instr)
{
    stringstream output;
    bool is_loading = instr & (1 << 11);
    bool is_byte = instr & (1 << 10);

    if (is_loading)
        output << "ldr";
    else
        output << "str";

    if (is_byte)
        output << "b";

    int offset = (instr >> 6) & 0x7;
    int base = (instr >> 3) & 0x7;
    int source_dest = instr & 0x7;

    output << " " << ARM_CPU::get_reg_name(source_dest) << ", [" << ARM_CPU::get_reg_name(base);
    output << ", " << ARM_CPU::get_reg_name(offset) << "]";
    return output.str();
}

string thumb_load_store_halfword(uint16_t instr)
{
    stringstream output;
    uint32_t offset = ((instr >> 6) & 0x1F) << 1;
    uint32_t base = (instr >> 3) & 0x7;
    uint32_t source_dest = instr & 0x7;
    bool is_loading = instr & (1 << 11);

    if (is_loading)
        output << "ldrh";
    else
        output << "strh";

    output << " " << ARM_CPU::get_reg_name(source_dest) << ", [" << ARM_CPU::get_reg_name(base);
    if (offset)
        output << ", #0x" << std::hex << offset;
    output << "]";

    return output.str();
}

string thumb_load_store_sign_halfword(uint16_t instr)
{
    const static string ops[] = {"strh", "ldsb", "ldrh", "ldsh"};
    stringstream output;
    uint32_t destination = instr & 0x7;
    uint32_t base = (instr >> 3) & 0x7;
    uint32_t offset = (instr >> 6) & 0x7;
    int opcode = (instr >> 10) & 0x3;

    output << ops[opcode] << " " << ARM_CPU::get_reg_name(destination) << ", [" << ARM_CPU::get_reg_name(base);
    output << ", " << ARM_CPU::get_reg_name(offset) << "]";
    return output.str();
}

string thumb_load_store_block(uint16_t instr)
{
    stringstream output;
    uint8_t reg_list = instr & 0xFF;
    uint32_t base = (instr >> 8) & 0x7;
    bool is_loading = instr & (1 << 11);

    if (is_loading)
        output << "ldmia";
    else
        output << "stmia";

    output << " " << ARM_CPU::get_reg_name(base) << "!, {";

    int consecutive_regs = 0;
    bool on_first_reg = true;
    for (int i = 0; i <= 8; i++)
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
    return output.str();
}

string thumb_push_pop(uint16_t instr)
{
    stringstream output;
    bool is_loading = instr & (1 << 11);
    bool use_LR_PC = instr & (1 << 8);
    uint8_t reg_list = instr & 0xFF;

    if (is_loading)
        output << "pop";
    else
        output << "push";

    output << " {";
    int consecutive_regs = 0;
    bool on_first_reg = true;

    //<= is used here so that consecutive regs ending at r7 will output correctly
    //e.g. push {r4-r7}
    //push {} would be outputted without <=
    for (int i = 0; i <= 8; i++)
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
    if (use_LR_PC)
    {
        if (!on_first_reg)
            output << ", ";
        if (is_loading)
            output << "pc";
        else
            output << "lr";
    }
    output << "}";
    return output.str();
}

string thumb_pc_rel_load(ARM_CPU &cpu, uint16_t instr)
{
    stringstream output;
    uint32_t destination = (instr >> 8) & 0x7;
    uint32_t address = cpu.get_PC() + ((instr & 0xFF) << 2);
    address &= ~0x3;
    output << "ldr " << ARM_CPU::get_reg_name(destination) << ", =0x" << std::hex << cpu.read32(address);
    return output.str();
}

string thumb_load_addr(ARM_CPU &cpu, uint16_t instr)
{
    stringstream output;
    uint32_t destination = (instr >> 8) & 0x7;
    uint32_t offset = (instr & 0xFF) << 2;
    bool adding_SP = instr & (1 << 11);

    if (adding_SP)
        output << "add " << ARM_CPU::get_reg_name(destination) << ", sp, #0x" << std::hex << offset;
    else
    {
        output << "adr " << ARM_CPU::get_reg_name(destination) << ", $" << std::hex << cpu.get_PC() + offset;
    }

    return output.str();
}

string thumb_sp_rel_load_store(uint16_t instr)
{
    stringstream output;
    uint32_t source_dest = (instr >> 8) & 0x7;
    uint32_t offset = (instr & 0xFF) << 2;
    bool is_loading = instr & (1 << 11);

    if (is_loading)
        output << "ldr";
    else
        output << "str";

    output << " " << ARM_CPU::get_reg_name(source_dest) << ", [sp";
    if (offset)
        output << ", #0x" << std::hex << offset;
    output << "]";
    return output.str();
}

string thumb_offset_sp(uint16_t instr)
{
    stringstream output;
    uint16_t offset = (instr & 0x7F) << 2;

    if (instr & (1 << 7))
        output << "sub";
    else
        output << "add";

    output << " sp, #0x" << std::hex << offset;

    return output.str();
}

string thumb_extend_op(uint16_t instr)
{
    static string ops[] = {"sxth", "sxtb", "uxth", "uxtb"};

    uint32_t dest = instr & 0x7;
    uint32_t source = (instr >> 3) & 0x7;

    stringstream output;
    output << ops[(instr >> 6) & 0x3] << " ";
    output << ARM_CPU::get_reg_name(dest) << ", " << ARM_CPU::get_reg_name(source);
    return output.str();
}

string thumb_branch(ARM_CPU &cpu, uint16_t instr)
{
    stringstream output;
    uint32_t address = cpu.get_PC();
    int16_t offset = (instr & 0x7FF) << 1;

    //Sign extend 12-bit offset
    offset <<= 4;
    offset >>= 4;

    address += offset;

    output << "b $0x" << std::hex << address;
    return output.str();
}

string thumb_cond_branch(ARM_CPU& cpu, uint16_t instr)
{
    stringstream output;
    int condition = (instr >> 8) & 0xF;
    if (condition == 0xF)
    {
        output << "swi 0x" << std::hex << (instr & 0xFF);
        return output.str();
    }

    uint32_t address = cpu.get_PC();
    int16_t offset = static_cast<int32_t>(instr << 24) >> 23;

    output << "b" << cond_name(condition) << " $0x";
    output << std::hex << (address + offset);
    return output.str();
}

string thumb_long_branch(ARM_CPU &cpu, uint16_t instr)
{
    stringstream output;
    uint32_t long_instr = instr | (cpu.read16(cpu.get_PC() - 2) << 16);
    bool switch_ARM = (long_instr & (1 << 28)) == 0;
    uint32_t address = cpu.get_PC();
    int32_t offset = (int32_t)((long_instr & 0x7FF) << 21) >> 9;
    address += offset;
    address += ((long_instr >> 16) & 0x7FF) << 1;

    if (switch_ARM)
    {
        output << "blx";
        address &= ~0x3;
    }
    else
        output << "bl";
    output << " $0x" << std::hex << address;
    return output.str();
}

};
