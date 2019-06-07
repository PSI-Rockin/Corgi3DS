#include <cstdio>
#include "arm.hpp"
#include "cp15.hpp"
#include "mmu.hpp"

CP15::CP15(int id, ARM_CPU* cpu, MMU* mmu) : id(id), cpu(cpu), mmu(mmu)
{

}

void CP15::reset(bool has_tcm)
{
    if (has_tcm)
    {
        itcm_size = 0x08000000;
        dtcm_base = 0xFFF00000;
        dtcm_size = 0x4000;

        high_exception_vector = true;

        for (uint32_t i = 0; i < itcm_size; i += 1024 * 32)
            mmu->add_physical_mapping(ITCM, i, 1024 * 32);
        mmu->add_physical_mapping(DTCM, dtcm_base, dtcm_size);
    }
    else
    {
        itcm_size = 0;
        dtcm_base = 0;
        dtcm_size = 0;

        high_exception_vector = false;
    }
}

uint8_t** CP15::get_tlb_mapping()
{
    if (!mmu_enabled)
        return mmu->get_direct_mapping();
    return mmu->get_privileged_mapping();
}

bool CP15::has_high_exceptions()
{
    return high_exception_vector;
}

uint32_t CP15::mrc(int operation_mode, int CP_reg, int coprocessor_info, int coprocessor_operand)
{
    //Don't know if operation mode is used for anything. Let's just keep it around for now
    (void)operation_mode;
    uint16_t op = (CP_reg << 8) | (coprocessor_operand << 4) | coprocessor_info;
    switch (op)
    {
        case 0x005:
            printf("[CP15_%d] Read CPU id\n", id);
            return id;
        case 0x100:
            return mmu_enabled;
        default:
            printf("[CP15_%d] Unrecognized MRC op $%04X\n", id, op);
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
        case 0x100:
            if (id != 9 && (value & 0x1))
                mmu->reload_tlb();
            mmu_enabled = value & 0x1;
            high_exception_vector = value & (1 << 13);
            break;
        case 0x200:
            if (id != 9)
                mmu->set_l1_table_base(0, value & ~0x3FFF);
            break;
        case 0x201:
            if (id != 9)
                mmu->set_l1_table_base(1, value & ~0x3FFF);
            break;
        case 0x502:
            if (id == 9)
                mmu->set_pu_permissions_ex(true, value);
            break;
        case 0x503:
            if (id == 9)
                mmu->set_pu_permissions_ex(false, value);
            break;
        case 0x600:
        case 0x610:
        case 0x620:
        case 0x630:
        case 0x640:
        case 0x650:
        case 0x660:
        case 0x670:
            if (id == 9)
                mmu->set_pu_region((op - 0x600) / 0x10, value);
            break;
        case 0x704:
            cpu->halt();
            break;
        case 0x7A4:
            break;
        case 0x7E1:
            break;
        case 0x870:
            if (id != 9 && mmu_enabled)
                mmu->reload_tlb();
            break;
        default:
            printf("[CP15_%d] Unrecognized MCR op $%04X ($%08X)\n", id, op, value);
    }
}
