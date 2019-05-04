#include <cstdlib>
#include <cstdio>
#include "arm.hpp"
#include "arm_disasm.hpp"
#include "arm_interpret.hpp"

namespace ARM_Interpreter
{

void interpret_arm(ARM_CPU &cpu, uint32_t instr)
{
    int cond = instr >> 28;

    if (cond == 0xF && (instr & 0xFE000000) == 0xFA000000)
    {
        arm_blx(cpu, instr);
        return;
    }

    if (!cpu.meets_condition(cond))
        return;

    switch (decode_arm(instr))
    {
        case ARM_B:
        case ARM_BL:
            arm_b(cpu, instr);
            break;
        case ARM_BX:
            arm_bx(cpu, instr);
            break;
        case ARM_BLX:
            arm_blx_reg(cpu, instr);
            break;
        case ARM_CLZ:
            arm_clz(cpu, instr);
            break;
        case ARM_DATA_PROCESSING:
            arm_data_processing(cpu, instr);
            break;
        case ARM_MULTIPLY:
            arm_mul(cpu, instr);
            break;
        case ARM_MULTIPLY_LONG:
            arm_mul_long(cpu, instr);
            break;
        case ARM_LOAD_BYTE:
            arm_load_byte(cpu, instr);
            break;
        case ARM_STORE_BYTE:
            arm_store_byte(cpu, instr);
            break;
        case ARM_LOAD_WORD:
            arm_load_word(cpu, instr);
            break;
        case ARM_STORE_WORD:
            arm_store_word(cpu, instr);
            break;
        case ARM_LOAD_BLOCK:
            arm_load_block(cpu, instr);
            break;
        case ARM_STORE_BLOCK:
            arm_store_block(cpu, instr);
            break;
        case ARM_COP_REG_TRANSFER:
            arm_cop_transfer(cpu, instr);
            break;
        case ARM_WFI:
            cpu.halt();
            break;
        default:
            printf("[ARM_Interpreter] Undefined instr $%08X\n", instr);
            exit(1);
    }
}

void arm_b(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t address = cpu.get_PC();
    int32_t offset = (instr & 0xFFFFFF) << 2;

    //Sign extend offset
    offset <<= 6;
    offset >>= 6;

    address += offset;

    if (instr & (1 << 24))
        cpu.set_register(REG_LR, cpu.get_PC() - 4);

    cpu.jp(address, false);
}

void arm_bx(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t reg_id = instr & 0xF;
    uint32_t new_address = cpu.get_register(reg_id);

    cpu.jp(new_address, true);
}

void arm_blx(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t address = cpu.get_PC();
    int32_t offset = (instr & 0xFFFFFF) << 2;

    offset <<= 6;
    offset >>= 6;

    if (instr & (1 << 24))
        offset += 2;

    cpu.set_register(REG_LR, address - 4);
    cpu.jp(address + offset + 1, true);
}

void arm_blx_reg(ARM_CPU &cpu, uint32_t instr)
{
    cpu.set_register(REG_LR, cpu.get_PC() - 4);

    uint32_t reg_id = instr & 0xF;

    uint32_t new_address = cpu.get_register(reg_id);
    cpu.jp(new_address, true);
}

void arm_clz(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t source = instr & 0xF;
    uint32_t destination = (instr >> 12) & 0xF;

    source = cpu.get_register(source);

    //Implementation from melonDS (I thought mine was wrong, but apparently it's correct)
    //Regardless I'm too lazy to change it, especially when my initial implementation was worse
    uint32_t bits = 0;
    while ((source & 0xFF000000) == 0)
    {
        bits += 8;
        source <<= 8;
        source |= 0xFF;
    }

    while ((source & 0x80000000) == 0)
    {
        bits++;
        source <<= 1;
        source |= 1;
    }

    cpu.set_register(destination, bits);
}

void arm_data_processing(ARM_CPU &cpu, uint32_t instr)
{
    int opcode = (instr >> 21) & 0xF;
    int first_operand = (instr >> 16) & 0xF;
    uint32_t first_operand_contents = cpu.get_register(first_operand);
    bool set_condition_codes = instr & (1 << 20);

    int destination = (instr >> 12) & 0xF;
    uint32_t second_operand;

    bool is_operand_imm = instr & (1 << 25);
    bool set_carry;
    int shift;

    switch (opcode)
    {
        case 0x0:
        case 0x1:
        case 0x8:
        case 0x9:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
            set_carry = set_condition_codes;
            break;
        default:
            set_carry = false;
            break;
    }

    if (is_operand_imm)
    {
        //Immediate values are rotated right
        second_operand = instr & 0xFF;
        //shift = (instr >> 8) & 0xF;
        shift = (instr & 0xF00) >> 7;

        second_operand = cpu.rotr32(second_operand, shift, set_carry);
    }
    else
    {
        second_operand = instr & 0xF;
        second_operand = cpu.get_register(second_operand);
        int shift_type = (instr >> 5) & 0x3;

        if (instr & (1 << 4))
        {
            shift = cpu.get_register((instr >> 8) & 0xF);
            //cpu.add_internal_cycles(1); //Extra cycle due to SHIFT(reg) operation

            //PC must take into account pipelining
            if ((instr & 0xF) == 15)
                second_operand = cpu.get_PC() + 4;
        }
        else
            shift = (instr >> 7) & 0x1F;

        switch (shift_type)
        {
            case 0: //Logical shift left
                second_operand = cpu.lsl(second_operand, shift, set_carry);
                break;
            case 1: //Logical shift right
                if (shift || (instr & (1 << 4)))
                    second_operand = cpu.lsr(second_operand, shift, set_carry);
                else
                    second_operand = cpu.lsr_32(second_operand, set_carry);
                break;
            case 2: //Arithmetic shift right
                if (shift || (instr & (1 << 4)))
                    second_operand = cpu.asr(second_operand, shift, set_carry);
                else
                    second_operand = cpu.asr_32(second_operand, set_carry);
                break;
            case 3: //Rotate right
                if (shift == 0)
                    second_operand = cpu.rrx(second_operand, set_carry);
                else
                    second_operand = cpu.rotr32(second_operand, shift, set_carry);
                break;
            default:
                printf("[ARM_Interpreter] Invalid data processing shift: %d\n", shift_type);
                exit(1);
        }
    }

    switch (opcode)
    {
        case 0x0:
            cpu.andd(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0x1:
            cpu.eor(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0x2:
            cpu.sub(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0x3:
            //Same as SUB, but switch the order of the operands
            cpu.sub(destination, second_operand, first_operand_contents, set_condition_codes);
            break;
        case 0x4:
            cpu.add(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0x5:
            cpu.adc(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0x6:
            cpu.sbc(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0x7:
            cpu.sbc(destination, second_operand, first_operand_contents, set_condition_codes);
            break;
        case 0x8:
            if (set_condition_codes)
            {
                cpu.tst(first_operand_contents, second_operand);
            }
            else
                cpu.mrs(instr);
            break;
        case 0x9:
            if (set_condition_codes)
            {
                cpu.teq(first_operand_contents, second_operand);
            }
            else
                cpu.msr(instr);
            break;
        case 0xA:
            if (set_condition_codes)
            {
                cpu.cmp(first_operand_contents, second_operand);
            }
            else
                cpu.mrs(instr);
            break;
        case 0xB:
            if (set_condition_codes)
            {
                cpu.cmn(first_operand_contents, second_operand);
            }
            else
                cpu.msr(instr);
            break;
        case 0xC:
            cpu.orr(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0xD:
            cpu.mov(destination, second_operand, set_condition_codes);
            break;
        case 0xE:
            cpu.bic(destination, first_operand_contents, second_operand, set_condition_codes);
            break;
        case 0xF:
            cpu.mvn(destination, second_operand, set_condition_codes);
            break;
        default:
            printf("Data processing opcode $%01X not recognized\n", opcode);
    }
}

void arm_mul(ARM_CPU &cpu, uint32_t instr)
{
    bool accumulate = instr & (1 << 21);
    bool set_condition_codes = instr & (1 << 20);
    int destination = (instr >> 16) & 0xF;
    int first_operand = instr & 0xF;
    int second_operand = (instr >> 8) & 0xF;
    int third_operand = (instr >> 12) & 0xF;

    uint32_t result = cpu.get_register(first_operand) * cpu.get_register(second_operand);

    if (accumulate)
        result += cpu.get_register(third_operand);

    if (set_condition_codes)
    {
        cpu.set_zero_neg_flags(result);
    }

    cpu.set_register(destination, result);
}

void arm_mul_long(ARM_CPU &cpu, uint32_t instr)
{
    bool is_signed = instr & (1 << 22);
    bool accumulate = instr & (1 << 21);
    bool set_condition_codes = instr & (1 << 20);

    int dest_hi = (instr >> 16) & 0xF;
    int dest_lo = (instr >> 12) & 0xF;
    uint32_t first_operand = (instr >> 8) & 0xF;
    uint32_t second_operand = (instr) & 0xF;

    first_operand = cpu.get_register(first_operand);
    second_operand = cpu.get_register(second_operand);

    if (is_signed)
    {
        int64_t result = static_cast<int64_t>((int32_t)first_operand) * static_cast<int64_t>((int32_t)second_operand);

        if (accumulate)
        {
            int64_t big_reg = cpu.get_register(dest_lo);
            big_reg |= static_cast<int64_t>(cpu.get_register(dest_hi)) << 32;

            result += big_reg;
        }

        cpu.set_register(dest_lo, result & 0xFFFFFFFF);
        cpu.set_register(dest_hi, result >> 32ULL);

        if (set_condition_codes)
        {
            cpu.set_zero(!result);
            cpu.set_neg(result >> 63);
        }
    }
    else
    {
        uint64_t result = static_cast<uint64_t>(first_operand) * static_cast<uint64_t>(second_operand);

        if (accumulate)
        {
            uint64_t big_reg = cpu.get_register(dest_lo);
            big_reg |= static_cast<uint64_t>(cpu.get_register(dest_hi)) << 32;

            result += big_reg;
        }

        cpu.set_register(dest_lo, result & 0xFFFFFFFF);
        cpu.set_register(dest_hi, result >> 32ULL);

        if (set_condition_codes)
        {
            cpu.set_zero(!result);
            cpu.set_neg(result >> 63);
        }
    }
}

uint32_t load_store_shift_reg(ARM_CPU& cpu, uint32_t instr)
{
    int reg = cpu.get_register(instr & 0xF);
    int shift_type = (instr >> 5) & 0x3;
    int shift = (instr >> 7) & 0x1F;

    switch (shift_type)
    {
        case 0: //Logical shift left
            reg = cpu.lsl(reg, shift, false);
            break;
        case 1: //Logical shift right
            if (!shift)
                shift = 32;
            reg = cpu.lsr(reg, shift, false);
            break;
        case 2: //Arithmetic shift right
            if (!shift)
                shift = 32;
            reg = cpu.asr(reg, shift, false);
            break;
        case 3: //Rotate right
            if (!shift)
                reg = cpu.rrx(reg, false);
            else
                reg = cpu.rotr32(reg, shift, false);
            break;
        default:
            printf("[ARM_Interpreter] Invalid load/store shift: %d\n", shift_type);
    }

    return reg;
}

void arm_load_byte(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t destination = (instr >> 12) & 0xF;
    uint32_t offset;

    bool is_imm = (instr & (1 << 25)) == 0;
    bool is_preindexing = instr & (1 << 24);
    bool is_adding_offset = instr & (1 << 23);
    bool is_writing_back = instr & (1 << 21);

    if (is_imm)
        offset = instr & 0xFFF;
    else
        offset = load_store_shift_reg(cpu, instr);

    uint32_t address = cpu.get_register(base);
    //cpu.add_n16_data(address, 1);
    //cpu.add_internal_cycles(1);

    if (is_preindexing)
    {
        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        if (is_writing_back)
            cpu.set_register(base, address);

        cpu.set_register(destination, cpu.read8(address));
    }
    else
    {
        cpu.set_register(destination, cpu.read8(address));

        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        if (base != destination)
            cpu.set_register(base, address);
    }
}

void arm_store_byte(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t source = (instr >> 12) & 0xF;
    uint32_t offset;

    bool is_imm = (instr & (1 << 25)) == 0;
    bool is_preindexing = (instr & (1 << 24)) != 0;
    bool is_adding_offset = (instr & (1 << 23)) != 0;
    bool is_writing_back = (instr & (1 << 21)) != 0;

    if (is_imm)
        offset = instr & 0xFFF;
    else
        offset = load_store_shift_reg(cpu, instr);

    uint32_t address = cpu.get_register(base);
    uint8_t value = cpu.get_register(source) & 0xFF;

    //cpu.add_n16_data(address, 1);

    if (is_preindexing)
    {
        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        if (is_writing_back)
            cpu.set_register(base, address);

        cpu.write8(address, value);
    }
    else
    {
        cpu.write8(address, value);

        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        cpu.set_register(base, address);
    }
}

void arm_load_word(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t destination = (instr >> 12) & 0xF;
    uint32_t offset;

    bool is_imm = (instr & (1 << 25)) == 0;
    bool is_preindexing = (instr & (1 << 24)) != 0;
    bool is_adding_offset = (instr & (1 << 23)) != 0;
    bool is_writing_back = (instr & (1 << 21)) != 0;

    if (is_imm)
        offset = instr & 0xFFF;
    else
        offset = load_store_shift_reg(cpu, instr);

    uint32_t address = cpu.get_register(base);
    //cpu.add_n32_data(address, 1);

    if (is_preindexing)
    {
        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        if (is_writing_back)
            cpu.set_register(base, address);

        //TODO: What does ARM11 do on unaligned access?
        uint32_t word = cpu.rotr32(cpu.read32(address & ~0x3), (address & 0x3) * 8, false);

        if (destination == REG_PC)
            cpu.jp(word, true);
        else
            cpu.set_register(destination, word);
    }
    else
    {
        uint32_t word = cpu.rotr32(cpu.read32(address & ~0x3), (address & 0x3) * 8, false);

        if (destination == REG_PC)
            cpu.jp(word, true);
        else
            cpu.set_register(destination, word);

        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        if (base != destination)
            cpu.set_register(base, address);
    }
}

void arm_store_word(ARM_CPU &cpu, uint32_t instr)
{
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t source = (instr >> 12) & 0xF;
    uint32_t offset;

    bool is_imm = (instr & (1 << 25)) == 0;
    bool is_preindexing = (instr & (1 << 24)) != 0;
    bool is_adding_offset = (instr & (1 << 23)) != 0;
    bool is_writing_back = (instr & (1 << 21)) != 0;

    if (is_imm)
        offset = instr & 0xFFF;
    else
        offset = load_store_shift_reg(cpu, instr);

    uint32_t address = cpu.get_register(base);
    uint32_t value = cpu.get_register(source);

    if (is_preindexing)
    {
        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        if (is_writing_back)
            cpu.set_register(base, address);

        //cpu.add_n32_data(address, 1);
        cpu.write32(address & ~0x3, value);
    }
    else
    {
        //cpu.add_n32_data(address, 1);
        cpu.write32(address & ~0x3, value);

        if (is_adding_offset)
            address += offset;
        else
            address -= offset;

        cpu.set_register(base, address);
    }
}

void arm_load_block(ARM_CPU &cpu, uint32_t instr)
{
    uint16_t reg_list = instr & 0xFFFF;
    uint32_t base = (instr >> 16) & 0xF;
    bool is_writing_back = instr & (1 << 21);
    bool load_PSR = instr & (1 << 22);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);
    bool user_bank_transfer = load_PSR && !(reg_list & (1 << 15));

    uint32_t address = cpu.get_register(base);
    int offset;
    if (is_adding_offset)
        offset = 4;
    else
        offset = -4;

    /*if (cpu.can_disassemble())
    {
        if (base == REG_SP && is_adding_offset && !is_preindexing && is_writing_back)
            printf("POP $%04X", reg_list);
        else
            printf("LDM {%d}, $%04X", base, reg_list);
    }*/

    PSR_Flags* cpsr = cpu.get_CPSR();
    PSR_MODE old_mode = cpsr->mode;
    if (user_bank_transfer)
    {
        cpu.update_reg_mode(PSR_USER);
        cpsr->mode = PSR_USER;
    }

    int regs = 0;
    if (is_adding_offset)
    {
        for (int i = 0; i < 15; i++)
        {
            int bit = 1 << i;
            if (reg_list & bit)
            {
                regs++;
                if (is_preindexing)
                {
                    address += offset;
                    cpu.set_register(i, cpu.read32(address));
                }
                else
                {
                    cpu.set_register(i, cpu.read32(address));
                    address += offset;
                }
            }
        }
        if (reg_list & (1 << 15))
        {
            if (is_preindexing)
            {
                address += offset;
                uint32_t new_PC = cpu.read32(address);
                cpu.jp(new_PC, true);
            }
            else
            {
                uint32_t new_PC = cpu.read32(address);
                cpu.jp(new_PC, true);
                address += offset;
            }
            if (load_PSR)
                cpu.spsr_to_cpsr();
            regs++;
        }
    }
    else
    {
        if (reg_list & (1 << 15))
        {
            if (is_preindexing)
            {
                address += offset;
                uint32_t new_PC = cpu.read32(address);
                cpu.jp(new_PC, true);
            }
            else
            {
                uint32_t new_PC = cpu.read32(address);
                cpu.jp(new_PC, true);
                address += offset;
            }
            if (load_PSR)
                cpu.spsr_to_cpsr();
            regs++;
        }
        for (int i = 14; i >= 0; i--)
        {
            int bit = 1 << i;
            if (reg_list & bit)
            {
                regs++;
                if (is_preindexing)
                {
                    address += offset;
                    cpu.set_register(i, cpu.read32(address));
                }
                else
                {
                    cpu.set_register(i, cpu.read32(address));
                    address += offset;
                }
            }
        }
    }

    if (user_bank_transfer)
    {
        cpu.update_reg_mode(old_mode);
        cpsr->mode = old_mode;
    }

    //if (regs > 1)
        //cpu.add_s32_data(address, regs - 1);
    //cpu.add_n32_data(address, 1);
    //cpu.add_internal_cycles(1);

    //TODO: ???
    if (is_writing_back && !((reg_list & (1 << base))))
        cpu.set_register(base, address);
}

void arm_store_block(ARM_CPU &cpu, uint32_t instr)
{
    uint16_t reg_list = instr & 0xFFFF;
    uint32_t base = (instr >> 16) & 0xF;
    bool is_writing_back = instr & (1 << 21);
    bool load_PSR = instr & (1 << 22);
    bool is_adding_offset = instr & (1 << 23);
    bool is_preindexing = instr & (1 << 24);
    bool user_bank_transfer = load_PSR && !(reg_list & (1 << 15));

    uint32_t address = cpu.get_register(base);
    int offset;
    if (is_adding_offset)
        offset = 4;
    else
        offset = -4;

    PSR_Flags* cpsr = cpu.get_CPSR();
    PSR_MODE old_mode = cpsr->mode;
    if (user_bank_transfer)
    {
        cpu.update_reg_mode(PSR_USER);
        cpsr->mode = PSR_USER;
    }

    int regs = 0;

    if (is_adding_offset)
    {
        for (int i = 0; i < 16; i++)
        {
            int bit = 1 << i;
            if (reg_list & bit)
            {
                regs++;
                if (is_preindexing)
                {
                    address += offset;
                    cpu.write32(address, cpu.get_register(i));
                }
                else
                {
                    cpu.write32(address, cpu.get_register(i));
                    address += offset;
                }
            }
        }
    }
    else
    {
        for (int i = 15; i >= 0; i--)
        {
            int bit = 1 << i;
            if (reg_list & bit)
            {
                regs++;
                if (is_preindexing)
                {
                    address += offset;
                    cpu.write32(address, cpu.get_register(i));
                }
                else
                {
                    cpu.write32(address, cpu.get_register(i));
                    address += offset;
                }
            }
        }
    }

    if (user_bank_transfer)
    {
        cpu.update_reg_mode(old_mode);
        cpsr->mode = old_mode;
    }

    //if (regs > 2)
        //cpu.add_s32_data(address, regs - 1);
    //cpu.add_n32_data(address, 2);
    if (is_writing_back)
        cpu.set_register(base, address);
}

void arm_cop_transfer(ARM_CPU &cpu, uint32_t instr)
{
    int operation_mode = (instr >> 21) & 0x7;
    bool is_loading = (instr & (1 << 20)) != 0;
    uint32_t CP_reg = (instr >> 16) & 0xF;
    uint32_t ARM_reg = (instr >> 12) & 0xF;

    int coprocessor_id = (instr >> 8) & 0xF;
    int coprocessor_info = (instr >> 5) & 0x7;
    int coprocessor_operand = instr & 0xF;

    if (is_loading)
    {
        uint32_t value = cpu.mrc(coprocessor_id, operation_mode, CP_reg, coprocessor_info, coprocessor_operand);
        cpu.set_register(ARM_reg, value);
    }
    else
    {
        //cpu.add_internal_cycles(1);
        //cpu.add_cop_cycles(1);
        uint32_t value = cpu.get_register(ARM_reg);
        cpu.mcr(coprocessor_id, operation_mode, CP_reg, coprocessor_info, coprocessor_operand, value);
    }
}

};
