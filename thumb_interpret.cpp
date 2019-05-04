#include <cstdlib>
#include <cstdio>
#include "arm.hpp"
#include "arm_interpret.hpp"
#include "arm_disasm.hpp"

namespace ARM_Interpreter
{

void interpret_thumb(ARM_CPU &cpu, uint16_t instr)
{
    switch (decode_thumb(instr))
    {
        case THUMB_MOV_SHIFT:
            thumb_move_shift(cpu, instr);
            break;
        case THUMB_ADD_REG:
            thumb_add_reg(cpu, instr);
            break;
        case THUMB_SUB_REG:
            thumb_sub_reg(cpu, instr);
            break;
        case THUMB_MOV_IMM:
            thumb_mov(cpu, instr);
            break;
        case THUMB_CMP_IMM:
            thumb_cmp(cpu, instr);
            break;
        case THUMB_ADD_IMM:
            thumb_add(cpu, instr);
            break;
        case THUMB_SUB_IMM:
            thumb_sub(cpu, instr);
            break;
        case THUMB_ALU_OP:
            thumb_alu(cpu, instr);
            break;
        case THUMB_HI_REG_OP:
            thumb_hi_reg_op(cpu, instr);
            break;
        case THUMB_LOAD_IMM_OFFSET:
            thumb_load_imm(cpu, instr);
            break;
        case THUMB_STORE_IMM_OFFSET:
            thumb_store_imm(cpu, instr);
            break;
        case THUMB_LOAD_REG_OFFSET:
            thumb_load_reg(cpu, instr);
            break;
        case THUMB_STORE_REG_OFFSET:
            thumb_store_reg(cpu, instr);
            break;
        case THUMB_LOAD_HALFWORD:
            thumb_load_halfword(cpu, instr);
            break;
        case THUMB_STORE_HALFWORD:
            thumb_store_halfword(cpu, instr);
            break;
        case THUMB_LOAD_STORE_SIGN_HALFWORD:
            thumb_load_store_signed(cpu, instr);
            break;
        case THUMB_LOAD_MULTIPLE:
            thumb_load_block(cpu, instr);
            break;
        case THUMB_STORE_MULTIPLE:
            thumb_store_block(cpu, instr);
            break;
        case THUMB_PUSH:
            thumb_push(cpu, instr);
            break;
        case THUMB_POP:
            thumb_pop(cpu, instr);
            break;
        case THUMB_PC_REL_LOAD:
            thumb_pc_rel_load(cpu, instr);
            break;
        case THUMB_LOAD_ADDRESS:
            thumb_load_addr(cpu, instr);
            break;
        case THUMB_SP_REL_LOAD:
            thumb_sp_rel_load(cpu, instr);
            break;
        case THUMB_SP_REL_STORE:
            thumb_sp_rel_store(cpu, instr);
            break;
        case THUMB_OFFSET_SP:
            thumb_offset_sp(cpu, instr);
            break;
        case THUMB_SXTH:
            thumb_sxth(cpu, instr);
            break;
        case THUMB_SXTB:
            thumb_sxtb(cpu, instr);
            break;
        case THUMB_UXTH:
            thumb_uxth(cpu, instr);
            break;
        case THUMB_UXTB:
            thumb_uxtb(cpu, instr);
            break;
        case THUMB_BRANCH:
            thumb_branch(cpu, instr);
            break;
        case THUMB_COND_BRANCH:
            thumb_cond_branch(cpu, instr);
            break;
        case THUMB_LONG_BRANCH_PREP:
            thumb_long_branch_prep(cpu, instr);
            break;
        case THUMB_LONG_BRANCH:
            thumb_long_branch(cpu, instr);
            break;
        case THUMB_LONG_BLX:
            thumb_long_blx(cpu, instr);
            break;
        default:
            printf("[Thumb_Interpreter] Undefined Thumb instr $%04X\n", instr);
            exit(1);
    }
}

void thumb_move_shift(ARM_CPU &cpu, uint16_t instr)
{
    int opcode = (instr >> 11) & 0x3;
    int shift = (instr >> 6) & 0x1F;
    uint32_t source = (instr >> 3) & 0x7;
    uint32_t destination = instr & 0x7;
    uint32_t value = cpu.get_register(source);

    switch (opcode)
    {
        case 0:
            value = cpu.lsl(value, shift, true);
            break;
        case 1:
            if (!shift)
                shift = 32;
            value = cpu.lsr(value, shift, true);
            break;
        case 2:
            if (!shift)
                shift = 32;
            value = cpu.asr(value, shift, true);
            break;
        default:
            printf("[Thumb_Interpreter] Unrecognized opcode %d in thumb_mov_shift\n", opcode);
            exit(1);
    }

    //cpu.add_internal_cycles(1); //Extra cycle due to register shift
    cpu.set_register(destination, value);
}

void thumb_add_reg(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = instr & 0x7;
    uint32_t source = (instr >> 3) & 0x7;
    uint32_t operand = (instr >> 6) & 0x7;
    bool is_imm = instr & (1 << 10);

    if (!is_imm)
        operand = cpu.get_register(operand);

    cpu.add(destination, cpu.get_register(source), operand, true);
}

void thumb_sub_reg(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = instr & 0x7;
    uint32_t source = (instr >> 3) & 0x7;
    uint32_t operand = (instr >> 6) & 0x7;
    bool is_imm = instr & (1 << 10);

    if (!is_imm)
        operand = cpu.get_register(operand);

    cpu.sub(destination, cpu.get_register(source), operand, true);
}

void thumb_mov(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t offset = instr & 0xFF;
    uint32_t reg = (instr >> 8) & 0x7;

    cpu.mov(reg, offset, true);
}

void thumb_cmp(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t offset = instr & 0xFF;
    uint32_t reg = (instr >> 8) & 0x7;

    cpu.cmp(cpu.get_register(reg), offset);
}

void thumb_add(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t offset = instr & 0xFF;
    uint32_t reg = (instr >> 8) & 0x7;

    cpu.add(reg, cpu.get_register(reg), offset, true);
}

void thumb_sub(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t offset = instr & 0xFF;
    uint32_t reg = (instr >> 8) & 0x7;

    cpu.sub(reg, cpu.get_register(reg), offset, true);
}

void thumb_alu(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = instr & 0x7;
    uint32_t source = (instr >> 3) & 0x7;
    int opcode = (instr >> 6) & 0xF;

    switch (opcode)
    {
        case 0x0:
            cpu.andd(destination, cpu.get_register(destination), cpu.get_register(source), true);
            break;
        case 0x1:
            cpu.eor(destination, cpu.get_register(destination), cpu.get_register(source), true);
            break;
        case 0x2:
        {
            uint32_t reg = cpu.get_register(destination);
            reg = cpu.lsl(reg, cpu.get_register(source), true);
            cpu.set_register(destination, reg);
        }
            break;
        case 0x3:
        {
            uint32_t reg = cpu.get_register(destination);
            reg = cpu.lsr(reg, cpu.get_register(source), true);
            cpu.set_register(destination, reg);
        }
            break;
        case 0x4:
        {
            uint32_t reg = cpu.get_register(destination);
            reg = cpu.asr(reg, cpu.get_register(source), true);
            cpu.set_register(destination, reg);
        }
            break;
        case 0x5:
            cpu.adc(destination, cpu.get_register(destination), cpu.get_register(source), true);
            break;
        case 0x6:
            cpu.sbc(destination, cpu.get_register(destination), cpu.get_register(source), true);
            break;
        case 0x7:
        {
            uint32_t reg = cpu.get_register(destination);
            reg = cpu.rotr32(reg, cpu.get_register(source), true);
            cpu.set_register(destination, reg);
        }
            break;
        case 0x8:
            cpu.tst(cpu.get_register(destination), cpu.get_register(source));
            break;
        case 0x9:
            //NEG is the same thing as RSBS Rd, Rs, #0
            cpu.sub(destination, 0, cpu.get_register(source), true);
            break;
        case 0xA:
            cpu.cmp(cpu.get_register(destination), cpu.get_register(source));
            break;
        case 0xB:
            cpu.cmn(cpu.get_register(destination), cpu.get_register(source));
            break;
        case 0xC:
            cpu.orr(destination, cpu.get_register(destination), cpu.get_register(source), true);
            break;
        case 0xD:
            cpu.mul(destination, cpu.get_register(destination), cpu.get_register(source), true);
            /*if (!cpu.get_id())
                cpu.add_internal_cycles(3);
            else
            {
                uint32_t multiplicand = cpu.get_register(destination);
                if (multiplicand & 0xFF000000)
                    cpu.add_internal_cycles(4);
                else if (multiplicand & 0x00FF0000)
                    cpu.add_internal_cycles(3);
                else if (multiplicand & 0x0000FF00)
                    cpu.add_internal_cycles(2);
                else
                    cpu.add_internal_cycles(1);
            }*/
            break;
        case 0xE:
            cpu.bic(destination, cpu.get_register(destination), cpu.get_register(source), true);
            break;
        case 0xF:
            cpu.mvn(destination, cpu.get_register(source), true);
            break;
        default:
            printf("Invalid thumb alu op %d\n", opcode);
    }
}

void thumb_hi_reg_op(ARM_CPU &cpu, uint16_t instr)
{
    int opcode = (instr >> 8) & 0x3;
    bool high_source = (instr & (1 << 6)) != 0;
    bool high_dest = (instr & (1 << 7)) != 0;

    uint32_t source = (instr >> 3) & 0x7;
    source |= high_source * 8;
    uint32_t destination = instr & 0x7;
    destination |= high_dest * 8;

    switch (opcode)
    {
        case 0x0:
            if (destination == REG_PC)
                cpu.jp(cpu.get_PC() + cpu.get_register(source), false);
            else
                cpu.add(destination, cpu.get_register(destination), cpu.get_register(source), false);
            break;
        case 0x1:
            cpu.cmp(cpu.get_register(destination), cpu.get_register(source));
            break;
        case 0x2:
            if (destination == REG_PC)
                cpu.jp(cpu.get_register(source), false);
            else
                cpu.mov(destination, cpu.get_register(source), false);
            break;
        case 0x3:
            if (high_dest)
                cpu.set_register(REG_LR, cpu.get_PC() - 1);
            cpu.jp(cpu.get_register(source), true);
            break;
        default:
            printf("High-reg Thumb opcode $%02X not recognized\n", opcode);
    }
}

void thumb_load_imm(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = instr & 0x7;
    uint32_t base = (instr >> 3) & 0x7;
    int offset = (instr >> 6) & 0x1F;
    bool is_byte = instr & (1 << 12);

    uint32_t address = cpu.get_register(base);

    if (is_byte)
    {
        address += offset;
        //cpu.add_n16_data(address, 1);
        cpu.set_register(destination, cpu.read8(address));
    }
    else
    {
        offset <<= 2;
        address += offset;
        //cpu.add_n32_data(address, 1);
        uint32_t word = cpu.rotr32(cpu.read32(address & ~0x3), (address & 0x3) * 8, false);
        cpu.set_register(destination, word);
    }
}

void thumb_store_imm(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t source = instr & 0x7;
    uint32_t base = (instr >> 3) & 0x7;
    int offset = (instr >> 6) & 0x1F;
    bool is_byte = instr & (1 << 12);

    uint32_t address = cpu.get_register(base);

    if (is_byte)
    {
        address += offset;
        //cpu.add_n16_data(address, 1);
        cpu.write8(address, cpu.get_register(source) & 0xFF);
    }
    else
    {
        offset <<= 2;
        address += offset;
        //cpu.add_n32_data(address, 1);
        cpu.write32(address, cpu.get_register(source));
    }
}

void thumb_load_reg(ARM_CPU &cpu, uint16_t instr)
{
    bool is_byte = (instr & (1 << 10)) != 0;

    uint32_t base = (instr >> 3) & 0x7;
    uint32_t destination = instr & 0x7;
    uint32_t offset = (instr >> 6) & 0x7;

    uint32_t address = cpu.get_register(base);
    address += cpu.get_register(offset);

    if (is_byte)
    {
        //cpu.add_n16_data(address, 1);
        //cpu.add_internal_cycles(1);
        cpu.set_register(destination, cpu.read8(address));
    }
    else
    {
        //cpu.add_n32_data(address, 1);
        //cpu.add_internal_cycles(1);
        uint32_t word = cpu.rotr32(cpu.read32(address & ~0x3), (address & 0x3) * 8, false);
        cpu.set_register(destination, word);
    }
}

void thumb_store_reg(ARM_CPU &cpu, uint16_t instr)
{
    bool is_byte = (instr & (1 << 10)) != 0;

    uint32_t base = (instr >> 3) & 0x7;
    uint32_t source = instr & 0x7;
    uint32_t offset = (instr >> 6) & 0x7;

    uint32_t address = cpu.get_register(base);
    address += cpu.get_register(offset);

    uint32_t source_contents = cpu.get_register(source);

    if (is_byte)
    {
        //cpu.add_n16_data(address, 1);
        cpu.write8(address, source_contents & 0xFF);
    }
    else
    {
        //cpu.add_n32_data(address, 1);
        cpu.write32(address, source_contents);
    }
}

void thumb_load_halfword(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t offset = ((instr >> 6) & 0x1F) << 1;
    uint32_t base = (instr >> 3) & 0x7;
    uint32_t destination = instr & 0x7;

    uint32_t address = cpu.get_register(base) + offset;

    //cpu.add_n16_data(address, 1);
    cpu.set_register(destination, cpu.read16(address));
}

void thumb_store_halfword(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t offset = ((instr >> 6) & 0x1F) << 1;
    uint32_t base = (instr >> 3) & 0x7;
    uint32_t source = instr & 0x7;

    uint32_t address = cpu.get_register(base) + offset;
    uint32_t value = cpu.get_register(source) & 0xFFFF;

    //cpu.add_n16_data(address, 1);
    cpu.write16(address, value);
}

void thumb_load_store_signed(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = instr & 0x7;
    uint32_t base = (instr >> 3) & 0x7;
    uint32_t offset = (instr >> 6) & 0x7;
    int opcode = (instr >> 10) & 0x3;

    uint32_t address = cpu.get_register(base);
    address += cpu.get_register(offset);

    switch (opcode)
    {
        case 0:
            cpu.write16(address, cpu.get_register(destination) & 0xFFFF);
            //cpu.add_n32_data(address, 1);
            break;
        case 1:
        {
            uint32_t extended_byte = cpu.read8(address);
            extended_byte = (int32_t)(int8_t)extended_byte;
            cpu.set_register(destination, extended_byte);
            //cpu.add_internal_cycles(1);
            //cpu.add_n16_data(address, 1);
        }
            break;
        case 2:
            cpu.set_register(destination, cpu.read16(address));
            //cpu.add_n16_data(address, 1);
            //cpu.add_internal_cycles(1);
            break;
        case 3:
        {
            uint32_t extended_halfword = cpu.read16(address);
            extended_halfword = (int32_t)(int16_t)extended_halfword;
            cpu.set_register(destination, extended_halfword);
            //cpu.add_internal_cycles(1);
            //cpu.add_n16_data(address, 1);
        }
            break;
        default:
            printf("\nSign extended opcode %d not recognized", opcode);
    }
}

void thumb_load_block(ARM_CPU &cpu, uint16_t instr)
{
    uint8_t reg_list = instr & 0xFF;
    uint32_t base = (instr >> 8) & 0x7;

    uint32_t address = cpu.get_register(base);

    int regs = 0;
    for (int reg = 0; reg < 8; reg++)
    {
        int bit = 1 << reg;
        if (reg_list & bit)
        {
            regs++;
            cpu.set_register(reg, cpu.read32(address));
            address += 4;
        }
    }

    //cpu.add_n32_data(address, 2);
    //if (regs > 1)
        //cpu.add_s32_data(address, regs - 2);
    //cpu.add_internal_cycles(1);

    int base_bit = 1 << base;
    if (!(reg_list & base_bit))
        cpu.set_register(base, address);
}

void thumb_store_block(ARM_CPU &cpu, uint16_t instr)
{
    uint8_t reg_list = instr & 0xFF;
    uint32_t base = (instr >> 8) & 0x7;

    uint32_t address = cpu.get_register(base);

    int regs = 0;
    for (int reg = 0; reg < 8; reg++)
    {
        int bit = 1 << reg;
        if (reg_list & bit)
        {
            regs++;
            cpu.write32(address, cpu.get_register(reg));
            address += 4;
        }
    }

    //cpu.add_n32_data(address, 2);
    //if (regs > 2)
        //cpu.add_s32_data(address, regs - 2);
    cpu.set_register(base, address);
}

void thumb_push(ARM_CPU &cpu, uint16_t instr)
{
    int reg_list = instr & 0xFF;
    uint32_t stack_pointer = cpu.get_register(REG_SP);

    int regs = 0;
    if (instr & (1 << 8))
    {
        regs++;
        stack_pointer -= 4;
        cpu.write32(stack_pointer, cpu.get_register(REG_LR));
    }

    for (int i = 7; i >= 0; i--)
    {
        int bit = 1 << i;
        if (reg_list & bit)
        {
            regs++;
            stack_pointer -= 4;
            cpu.write32(stack_pointer, cpu.get_register(i));
        }
    }

    //cpu.add_n32_data(stack_pointer, 2);
    //if (regs > 2)
        //cpu.add_s32_data(stack_pointer, regs - 2);

    cpu.set_register(REG_SP, stack_pointer);
}

void thumb_pop(ARM_CPU &cpu, uint16_t instr)
{
    int reg_list = instr & 0xFF;
    uint32_t stack_pointer = cpu.get_register(REG_SP);

    int regs = 0;
    for (int i = 0; i < 8; i++)
    {
        int bit = 1 << i;
        if (reg_list & bit)
        {
            regs++;
            cpu.set_register(i, cpu.read32(stack_pointer));
            stack_pointer += 4;
        }
    }

    if (instr & (1 << 8))
    {
        cpu.jp(cpu.read32(stack_pointer), true);

        regs++;
        stack_pointer += 4;
    }

    /*if (regs > 1)
        cpu.add_s32_data(stack_pointer, regs - 1);
    cpu.add_n32_data(stack_pointer, 1);
    cpu.add_internal_cycles(1);*/
    cpu.set_register(REG_SP, stack_pointer);
}

void thumb_pc_rel_load(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = (instr >> 8) & 0x7;
    uint32_t address = cpu.get_PC() + ((instr & 0xFF) << 2);
    address &= ~0x3; //keep memory aligned safely

    //cpu.add_n32_data(address, 1);
    //cpu.add_internal_cycles(1);
    cpu.set_register(destination, cpu.read32(address));
}

void thumb_load_addr(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = (instr >> 8) & 0x7;
    uint32_t offset = (instr & 0xFF) << 2;
    bool adding_SP = instr & (1 << 11);

    uint32_t address;
    if (adding_SP)
        address = cpu.get_register(REG_SP);
    else
        address = cpu.get_PC() & ~0x2; //Set bit 1 to zero for alignment safety

    cpu.add(destination, address, offset, false);
}

void thumb_sp_rel_load(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t destination = (instr >> 8) & 0x7;
    uint32_t offset = (instr & 0xFF) << 2;

    uint32_t address = cpu.get_register(REG_SP);
    address += offset;

    //cpu.add_n32_data(address, 1);
    //cpu.add_internal_cycles(1);
    cpu.set_register(destination, cpu.read32(address));
}

void thumb_sp_rel_store(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t source = (instr >> 8) & 0x7;
    uint32_t offset = (instr & 0xFF) << 2;

    uint32_t address = cpu.get_register(REG_SP);
    address += offset;

    //cpu.add_n32_data(address, 1);
    cpu.write32(address, cpu.get_register(source));
}

void thumb_offset_sp(ARM_CPU &cpu, uint16_t instr)
{
    int16_t offset = (instr & 0x7F) << 2;

    if (instr & (1 << 7))
        offset = -offset;

    uint32_t sp = cpu.get_register(REG_SP);
    cpu.set_register(REG_SP, sp + offset);
}

void thumb_sxth(ARM_CPU &cpu, uint16_t instr)
{
    int dest = instr & 0x7;
    int source = (instr >> 3) & 0x7;
    int32_t source_reg = (int16_t)(cpu.get_register(source) & 0xFFFF);

    cpu.set_register(dest, source_reg & 0xFFFF);
}

void thumb_sxtb(ARM_CPU &cpu, uint16_t instr)
{
    int dest = instr & 0x7;
    int source = (instr >> 3) & 0x7;
    int32_t source_reg = (int8_t)(cpu.get_register(source) & 0xFF);

    cpu.set_register(dest, source_reg & 0xFF);
}

void thumb_uxth(ARM_CPU &cpu, uint16_t instr)
{
    int dest = instr & 0x7;
    int source = (instr >> 3) & 0x7;
    uint32_t source_reg = cpu.get_register(source);

    cpu.set_register(dest, source_reg & 0xFFFF);
}

void thumb_uxtb(ARM_CPU &cpu, uint16_t instr)
{
    int dest = instr & 0x7;
    int source = (instr >> 3) & 0x7;
    uint32_t source_reg = cpu.get_register(source);

    cpu.set_register(dest, source_reg & 0xFF);
}

void thumb_branch(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t address = cpu.get_PC();
    int16_t offset = (instr & 0x7FF) << 1;

    //Sign extend 12-bit offset
    offset <<= 4;
    offset >>= 4;

    address += offset;

    cpu.jp(address, false);
}

void thumb_cond_branch(ARM_CPU &cpu, uint16_t instr)
{
    int condition = (instr >> 8) & 0xF;
    if (condition == 0xF)
    {
        printf("[Thumb_Interpreter] SWI not implemented\n");
        exit(1);
        //cpu.handle_SWI();
        return;
    }

    uint32_t address = cpu.get_PC();
    int16_t offset = static_cast<int32_t>(instr << 24) >> 23;

    if (cpu.meets_condition(condition))
        cpu.jp(address + offset, false);
}

void thumb_long_branch_prep(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t upper_address = cpu.get_PC();
    int32_t offset = ((instr & 0x7FF) << 21) >> 9;
    upper_address += offset;

    cpu.set_register(REG_LR, upper_address);
}

void thumb_long_branch(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t address = cpu.get_register(REG_LR);
    address += (instr & 0x7FF) << 1;

    uint32_t new_LR = cpu.get_PC() - 2;
    new_LR |= 0x1; //Preserve Thumb mode

    cpu.set_register(REG_LR, new_LR);

    cpu.jp(address, false);
}

void thumb_long_blx(ARM_CPU &cpu, uint16_t instr)
{
    uint32_t address = cpu.get_register(REG_LR);
    address += (instr & 0x7FF) << 1;

    uint32_t new_LR = cpu.get_PC() - 2;
    new_LR |= 0x1; //Preserve Thumb mode

    cpu.set_register(REG_LR, new_LR);

    cpu.jp(address, true); //Switch to ARM mode
}

};
