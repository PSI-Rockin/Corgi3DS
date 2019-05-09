#include <cstdio>
#include "arm.hpp"
#include "cp15.hpp"

CP15::CP15(int id, ARM_CPU* cpu) : id(id), cpu(cpu)
{

}

void CP15::reset(bool has_tcm)
{
    if (has_tcm)
    {
        itcm_size = 0x08000000;
        dtcm_base = 0xFFF00000;
        dtcm_size = 0x4000;
    }
    else
    {
        itcm_size = 0;
        dtcm_base = 0;
        dtcm_size = 0;
    }
}

uint32_t CP15::mrc(int operation_mode, int CP_reg, int coprocessor_info, int coprocessor_operand)
{
    //Don't know if operation mode is used for anything. Let's just keep it around for now
    (void)operation_mode;
    uint16_t op = (CP_reg << 8) | (coprocessor_operand << 4) | coprocessor_info;
    switch (op)
    {
        case 0x050:
            return id;
        default:
            printf("[CP15] Unrecognized MRC op $%04X\n", op);
            return 0;
    }
}

void CP15::mcr(int operation_mode, int CP_reg, int coprocessor_info, int coprocessor_operand, uint32_t value)
{
    (void)operation_mode;
    (void)value;
    uint16_t op = (CP_reg << 8) | (coprocessor_operand << 4) | coprocessor_info;
    switch (op)
    {
        case 0x704:
            cpu->halt();
            break;
        case 0x7A4:
            break;
        case 0x7E1:
            break;
        default:
            printf("[CP15] Unrecognized MCR op $%04X\n", op);
    }
}
