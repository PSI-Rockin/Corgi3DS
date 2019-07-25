#include <cstdio>
#include "../common/common.hpp"
#include "arm.hpp"
#include "cp15.hpp"
#include "mmu.hpp"

CP15::CP15(int id, ARM_CPU* cpu, MMU* mmu) : id(id), cpu(cpu), mmu(mmu)
{

}

void CP15::reset(bool has_tcm)
{
    mmu_enabled = false;
    aux_control = 0xF;
    data_fault_addr = 0;
    data_fault_reg = 0;
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

void CP15::reload_tlb(uint32_t addr)
{
    if (mmu_enabled)
    {
        if (id != 9)
            mmu->reload_tlb(addr);
        else
            mmu->reload_pu();
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

void CP15::set_data_abort_regs(uint32_t vaddr, bool is_write)
{
    data_fault_addr = vaddr;

    data_fault_reg = 0x5; //translation fault
    data_fault_reg |= is_write << 11;
}

void CP15::set_prefetch_abort_regs(uint32_t vaddr)
{
    instr_fault_reg = 0x5; //translation fault
}

uint32_t CP15::mrc(int operation_mode, int CP_reg, int coprocessor_info, int coprocessor_operand)
{
    //Don't know if operation mode is used for anything. Let's just keep it around for now
    (void)operation_mode;
    uint16_t op = (CP_reg << 8) | (coprocessor_operand << 4) | coprocessor_info;
    switch (op)
    {
        case 0x000:
            if (id != 9)
                return 0x410FB024;
            return 0;
        case 0x005:
            printf("[CP15_%d] Read CPU id\n", id);
            return id;
        case 0x014:
            if (id != 9)
                return 0x01100103;
            return 0;
        case 0x015:
            if (id != 9)
                return 0x10020302;
            return 0;
        case 0x016:
            if (id != 9)
                return 0x01222000;
            return 0;
        case 0x017:
            return 0;
        case 0x020:
            if (id != 9)
                return 0x00100011;
            return 0;
        case 0x021:
            if (id != 9)
                return 0x12002111;
            return 0;
        case 0x022:
            if (id != 9)
                return 0x11221011;
            return 0;
        case 0x023:
            if (id != 9)
                return 0x01102131;
            return 0;
        case 0x024:
            if (id != 9)
                return 0x00000141;
            return 0;
        case 0x100:
        {
            uint32_t reg = mmu_enabled;
            reg |= high_exception_vector << 13;
            return reg;
        }
        case 0x101:
            return aux_control;
        case 0x200:
        case 0x201:
            if (id != 9)
                return mmu->get_l1_table_base(op & 0x1);
            return 0;
        case 0x202:
            if (id != 9)
                return mmu->get_l1_table_control();
            return 0;
        case 0x300:
            return mmu->get_domain_control();
        case 0x500:
            return data_fault_reg;
        case 0x501:
            return instr_fault_reg;
        case 0x600:
            return data_fault_addr;
        case 0xD02:
        case 0xD03:
        case 0xD04:
            return thread_regs[op - 0xD02];
        default:
            printf("[CP15_%d] Unrecognized MRC op $%04X\n", id, op);
    }
    return 0;
}

void CP15::mcr(int operation_mode, int CP_reg, int coprocessor_info, int coprocessor_operand, uint32_t value)
{
    (void)operation_mode;
    (void)value;
    uint16_t op = (CP_reg << 8) | (coprocessor_operand << 4) | coprocessor_info;

    switch (op)
    {
        case 0x100:
            if (value & 0x1)
                reload_tlb(0);
            mmu_enabled = value & 0x1;
            high_exception_vector = value & (1 << 13);
            break;
        case 0x101:
            aux_control = value;
            break;
        case 0x200:
            if (id != 9)
                mmu->set_l1_table_base(0, value);
            break;
        case 0x201:
            if (id != 9)
                mmu->set_l1_table_base(1, value);
            break;
        case 0x202:
            if (id != 9)
                mmu->set_l1_table_control(value);
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
        case 0x750:
            break;
        case 0x761:
        case 0x7A1:
        case 0x7A4:
        case 0x7A5:
            break;
        case 0x7E1:
            break;
        case 0x850:
        case 0x860:
        case 0x870:
            if (id != 9)
            {
                printf("[CP15_%d] TLB invalidate\n", id);
                mmu->invalidate_tlb();
            }
            break;
        case 0x851:
        case 0x861:
        case 0x871:
            if (id != 9)
            {
                printf("[CP15_%d] TLB invalidate by addr: $%08X\n", id, value);
                mmu->invalidate_tlb_by_addr(value);
            }
            break;
        case 0x852:
        case 0x862:
        case 0x872:
            if (id != 9)
            {
                printf("[CP15_%d] TLB invalidate by ASID: $%08X\n", id, value);

                //Bit of a hack. We can assume that nonglobal entries only exist in table 0
                mmu->invalidate_tlb_by_table(0);
                //mmu->invalidate_tlb_by_asid(value & 0xFF);
            }
            break;
        case 0x910:
            if (id == 9)
            {
                mmu->remove_physical_mapping(dtcm_base, dtcm_size);

                dtcm_base = value & ~0xFFF;
                dtcm_size = (value >> 1) & 0x1F;
                dtcm_size = 512 << dtcm_size;

                mmu->add_physical_mapping(DTCM, dtcm_base, dtcm_size);
            }
            break;
        case 0xD01:
            if (id != 9)
                mmu->set_asid(value);
            break;
        case 0xD02:
        case 0xD03:
        case 0xD04:
            printf("[CP15_%d] Write thread reg%d: $%08X\n", id, op - 0xD02, value);
            thread_regs[op - 0xD02] = value;
            break;
        default:
            printf("[CP15_%d] Unrecognized MCR op $%04X ($%08X)\n", id, op, value);
    }
}
