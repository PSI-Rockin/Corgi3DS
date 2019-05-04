#include <cstdio>
#include "cp15.hpp"

CP15::CP15(int id) : id(id)
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
    uint16_t op = (CP_reg << 8) | (coprocessor_info << 4) | coprocessor_operand;
    switch (op)
    {
        case 0x005:
            return id;
        default:
            printf("[CP15] Unrecognized MRC op $%04X\n", op);
            return 0;
    }
}

void CP15::mcr(int operation_mode, int CP_reg, int coprocessor_info, int coprocessor_operand, uint32_t value)
{
    uint16_t op = (CP_reg << 8) | (coprocessor_info << 4) | coprocessor_operand;
    switch (op)
    {
        default:
            printf("[CP15] Unrecognized MCR op $%04X\n", op);
    }
}
