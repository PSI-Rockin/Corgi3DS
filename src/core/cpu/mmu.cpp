#include <cstring>
#include <cstdio>
#include "mmu.hpp"

MMU::MMU()
{
    user_mapping = nullptr;
    privileged_mapping = nullptr;
    direct_mapping = nullptr;
    asid_mapping = nullptr;
}

MMU::~MMU()
{
    delete[] user_mapping;
    delete[] privileged_mapping;
    delete[] direct_mapping;
    delete[] asid_mapping;
}

void MMU::reset()
{
    if (!user_mapping)
        user_mapping = new uint8_t*[1024 * 1024];
    if (!privileged_mapping)
        privileged_mapping = new uint8_t*[1024 * 1024];
    if (!direct_mapping)
        direct_mapping = new uint8_t*[1024 * 1024];
    if (!asid_mapping)
        asid_mapping = new uint16_t[1024 * 1024];

    whole_tlb_invalidated = true;

    domain_control = 0;

    memset(user_mapping, 0, 1024 * 1024 * sizeof(uint8_t*));
    memset(privileged_mapping, 0, 1024 * 1024 * sizeof(uint8_t*));

    for (uint64_t i = 0; i < 1024 * 1024; i++)
    {
        uint64_t addr = i * 4096;
        addr |= 0xFUL << 60UL;
        direct_mapping[i] = (uint8_t*)addr;
    }

    memset(asid_mapping, 0xFF, 1024 * 1024 * sizeof(uint16_t));

    l1_table_base[0] = 0;
    l1_table_base[1] = 0;
    set_l1_table_control(0);
    asid = 0;

    memset(pu_regions, 0, sizeof(pu_regions));
}

void MMU::add_physical_mapping(uint8_t *mem, uint32_t base, uint32_t size)
{
    uint64_t addr = (uint64_t)mem;
    addr |= 7UL << 60UL;

    //Convert to page offsets
    base /= 4096;
    size /= 4096;
    for (unsigned int i = base; i < base + size; i++)
    {
        direct_mapping[i] = (uint8_t*)addr;
        addr += 4096;
    }
}

void MMU::remove_physical_mapping(uint32_t base, uint32_t size)
{
    base /= 4096;
    size /= 4096;

    for (unsigned int i = base; i < base + size; i++)
    {
        uint64_t addr = i * 4096;
        addr |= 0xFUL << 60UL;
        direct_mapping[i] = (uint8_t*)addr;
    }
}

uint8_t** MMU::get_direct_mapping()
{
    return direct_mapping;
}

uint8_t** MMU::get_privileged_mapping()
{
    return privileged_mapping;
}

uint8_t** MMU::get_user_mapping()
{
    return user_mapping;
}

void MMU::invalidate_tlb()
{
    whole_tlb_invalidated = true;
    memset(privileged_mapping, 0, 1024 * 1024 * sizeof(uint8_t*));
    memset(user_mapping, 0, 1024 * 1024 * sizeof(uint8_t*));
    memset(asid_mapping, 0xFF, 1024 * 1024 * sizeof(uint16_t));
}

void MMU::invalidate_tlb_by_asid(uint8_t value)
{
    for (int i = 0; i < 1024 * 1024; i++)
    {
        if (asid_mapping[i] == value)
        {
            asid_mapping[i] = 0xFFFF;
            privileged_mapping[i] = nullptr;
            user_mapping[i] = nullptr;
        }
    }
}

void MMU::invalidate_tlb_by_addr(uint32_t value)
{
    uint32_t addr = value / 4096;

    if (asid_mapping[addr] == (value & 0xFF))
    {
        asid_mapping[addr] = 0xFFFF;
        privileged_mapping[addr] = nullptr;
        user_mapping[addr] = nullptr;
    }
}

void MMU::reload_tlb_section(uint32_t addr)
{
    uint32_t table_index = (addr / (1024 * 1024)) * 4;

    uint32_t base;
    if (table_index < l1_table_cutoff)
        base = l1_table_base[0];
    else
        base = l1_table_base[1];

    uint32_t l1_addr = base + table_index;
    uint32_t page = addr / 4096;

    uint64_t mem = (uint64_t)direct_mapping[l1_addr / 4096];
    mem &= ~(0xFUL << 60UL);
    uint8_t* ptr = (uint8_t*)mem;
    uint32_t entry = *(uint32_t*)&ptr[l1_addr & 0xFFF];
    addr &= ~0xFFFFF;

    uint32_t type = entry & 0x3;

    if (type == 0 || type == 3)
    {
        for (int j = 0; j < 256; j++)
        {
            privileged_mapping[page + j] = nullptr;
            user_mapping[page + j] = nullptr;
        }
    }
    else if (type == 2)
    {
        uint32_t paddr;
        bool exec_never = (entry & (1 << 4)) != 0;
        uint8_t apx = (entry >> 10) & 0x3;
        if (entry & (1 << 18))
        {
            paddr = entry & 0xFF000000;

            remap_mmu_region(addr, 1024 * 1024 * 16, paddr, apx, exec_never, false);
        }
        else
        {
            if (entry & (1 << 15))
                apx |= 1 << 2;
            bool nonglobal = entry & (1 << 17);
            paddr = entry & 0xFFF00000;

            remap_mmu_region(addr, 1024 * 1024, paddr, apx, exec_never, nonglobal);
        }
    }
    else
    {
        uint32_t l2_addr = entry & ~0x3FF;
        //printf("L2 table at $%08X\n", l2_addr);
        for (int i = 0; i < 1024; i += 4)
        {
            uint64_t l2_mem = (uint64_t)direct_mapping[(l2_addr + i) / 4096];
            l2_mem &= ~(0xFUL << 60UL);
            uint8_t* l2_ptr = (uint8_t*)l2_mem;
            uint32_t l2_entry = *(uint32_t*)&l2_ptr[(l2_addr + i) & 0xFFF];

            uint32_t type = l2_entry & 0x3;
            uint8_t apx = (l2_entry >> 4) & 0x3;
            if (l2_entry & (1 << 9))
                apx |= 1 << 2;

            bool nonglobal = l2_entry & (1 << 11);

            //printf("[$%08X] $%08X - ", addr, l2_entry);

            if (!type)
            {
                //printf("Unmapped\n");
                privileged_mapping[page] = nullptr;
                user_mapping[page] = nullptr;
                page++;
                addr += 1024 * 4;
            }
            else if (type == 1)
            {
                uint32_t paddr = l2_entry & 0xFFFF0000;

                //printf("64 KB $%08X (APX=%d) (nG=%d)\n", paddr, apx, nonglobal);
                remap_mmu_region(addr, 1024 * 64, paddr, apx, false, nonglobal);

                i += 60;
                page += 16;
                addr += 1024 * 64;
            }
            else
            {
                uint32_t paddr = l2_entry & ~0xFFF;
                bool exec_never = (l2_entry & 0x1) != 0;
                //printf("4 KB $%08X (XN=%d) (APX=%d) (nG=%d)\n", paddr, exec_never, apx, nonglobal);
                remap_mmu_region(addr, 1024 * 4, paddr, apx, exec_never, nonglobal);
                page++;
                addr += 1024 * 4;
            }
        }
    }
}

void MMU::reload_tlb_by_table(int index)
{
    if (!l1_table_base[index])
        return;

    uint32_t addr = l1_table_base[index];
    uint32_t size;
    uint32_t pc;

    if (index == 0)
    {
        size = l1_table_cutoff;
        pc = 0;
    }
    else
    {
        size = 1024 * 16;
        addr += l1_table_cutoff;
        pc = l1_table_cutoff * 1024 * 256;
    }

    //printf("Reloading TLB from $%08X... (size: $%08X)\n", addr, size);

    while (addr < l1_table_base[index] + size)
    {
        uint64_t mem = (uint64_t)direct_mapping[addr / 4096];
        mem &= ~(0xFUL << 60UL);
        uint8_t* ptr = (uint8_t*)mem;
        uint32_t entry = *(uint32_t*)&ptr[addr & 0xFFF];

        uint32_t type = entry & 0x3;
        //printf("[$%08X] $%08X - ", pc, entry);
        if (type == 0 || type == 3)
        {
            //printf("Unmapped\n");

            for (int j = 0; j < 256; j++)
            {
                privileged_mapping[(pc / 4096) + j] = nullptr;
                user_mapping[(pc / 4096) + j] = nullptr;
            }
            pc += 1024 * 1024;
        }
        else if (type == 2)
        {
            uint32_t paddr;
            bool exec_never = (entry & (1 << 4)) != 0;
            uint8_t apx = (entry >> 10) & 0x3;
            if (entry & (1 << 18))
            {
                paddr = entry & 0xFF000000;
                //printf("Supersection $%08X (XN=%d) (APX=%d)\n", paddr, exec_never, apx);

                remap_mmu_region(pc, 1024 * 1024 * 16, paddr, apx, exec_never, false);

                pc += 1024 * 1024 * 16;
                addr += 64 - 4;
            }
            else
            {
                if (entry & (1 << 15))
                    apx |= 1 << 2;
                bool nonglobal = entry & (1 << 17);
                paddr = entry & 0xFFF00000;
                //printf("Section $%08X (XN=%d) (APX=%d) (nG=%d)\n", paddr, exec_never, apx, nonglobal);

                remap_mmu_region(pc, 1024 * 1024, paddr, apx, exec_never, nonglobal);
                pc += 1024 * 1024;
            }
        }
        else
        {
            uint32_t l2_addr = entry & ~0x3FF;
            //printf("L2 table at $%08X\n", l2_addr);
            for (int i = 0; i < 1024; i += 4)
            {
                uint64_t l2_mem = (uint64_t)direct_mapping[(l2_addr + i) / 4096];
                l2_mem &= ~(0xFUL << 60UL);
                uint8_t* l2_ptr = (uint8_t*)l2_mem;
                uint32_t l2_entry = *(uint32_t*)&l2_ptr[(l2_addr + i) & 0xFFF];
                //printf("[$%08X] $%08X - ", pc, l2_entry);

                uint32_t type = l2_entry & 0x3;
                uint8_t apx = (l2_entry >> 4) & 0x3;
                if (l2_entry & (1 << 9))
                    apx |= 1 << 2;

                bool nonglobal = l2_entry & (1 << 11);

                if (!type)
                {
                    //printf("Unmapped\n");
                    privileged_mapping[pc / 4096] = nullptr;
                    user_mapping[pc / 4096] = nullptr;
                    pc += 1024 * 4;
                }
                else if (type == 1)
                {
                    uint32_t paddr = l2_entry & 0xFFFF0000;
                    //printf("64 KB $%08X (APX=%d) (nG=%d)\n", paddr, apx, nonglobal);

                    remap_mmu_region(pc, 1024 * 64, paddr, apx, false, nonglobal);

                    pc += 1024 * 64;
                    i += 60;
                }
                else
                {
                    uint32_t paddr = l2_entry & ~0xFFF;
                    bool exec_never = (l2_entry & 0x1) != 0;
                    //printf("4 KB $%08X (XN=%d) (APX=%d) (nG=%d)\n", paddr, exec_never, apx, nonglobal);
                    remap_mmu_region(pc, 1024 * 4, paddr, apx, exec_never, nonglobal);
                    pc += 1024 * 4;
                }
            }
        }
        addr += 4;
    }
}

void MMU::reload_tlb(uint32_t addr)
{
    if (whole_tlb_invalidated)
    {
        reload_tlb_by_table(0);
        reload_tlb_by_table(1);
        whole_tlb_invalidated = false;
    }
    else
        reload_tlb_section(addr);
}

void MMU::remap_mmu_region(uint32_t base, uint32_t size, uint64_t paddr,
                           uint8_t apx, bool exec_never, bool nonglobal)
{
    base /= 4096;
    size /= 4096;
    paddr /= 4096;

    //apx &= ~0x4;

    uint64_t priv_perm = get_privileged_apx_perms(apx);
    if (exec_never)
        priv_perm &= ~0x1;

    priv_perm <<= 60UL;

    for (unsigned int i = 0; i < size; i++)
    {
        uint64_t mapping = (uint64_t)direct_mapping[paddr + i];

        //Strip permission bits off
        mapping &= ~(0x7UL << 60UL);

        privileged_mapping[base + i] = (uint8_t*)(mapping | priv_perm);

        if (nonglobal)
            asid_mapping[base + i] = asid;
        else
            asid_mapping[base + i] = 0xFFFF;
    }
}

uint32_t MMU::get_l1_table_base(int index)
{
    return l1_table_base[index];
}

uint32_t MMU::get_l1_table_control()
{
    return l1_table_control;
}

uint32_t MMU::get_domain_control()
{
    return domain_control;
}

void MMU::set_l1_table_base(int index, uint32_t value)
{
    printf("[MMU] Set translation table%d base: $%08X\n", index, value);
    uint32_t mask = 0x3FFF;
    mask >>= l1_table_control;
    l1_table_base[index] = value & ~mask;
    invalidate_tlb();
}

void MMU::set_l1_table_control(uint32_t value)
{
    //If control is 0, translation table 0 will cover all of memory.
    printf("[MMU] Set control: $%08X\n", value);
    l1_table_control = value;
    l1_table_cutoff = 1UL << 32UL;
    l1_table_cutoff >>= value & 0x7;
    l1_table_cutoff /= (1024 * 256);
}

void MMU::set_domain_control(uint32_t value)
{
    domain_control = value;
}

void MMU::set_asid(uint32_t value)
{
    printf("[MMU] Set ASID: $%08X\n", value);
    asid = value & 0xFF;
}

void MMU::set_pu_permissions_ex(bool is_data, uint32_t value)
{
    printf("[MMU] Set PU permissions (data=%d): $%08X\n", is_data, value);
    for (int i = 0; i < 8; i++)
    {
        uint8_t type = (value >> (i * 4)) & 0xF;
        if (is_data)
        {
            const static MMU_Perm privileged_perms[16] =
                {
                MMU_NONE, MMU_RW, MMU_RW, MMU_RW, MMU_NONE, MMU_R, MMU_R, MMU_NONE,
                MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE
                };
            const static MMU_Perm user_perms[16] =
                {
                MMU_NONE, MMU_NONE, MMU_R, MMU_RW, MMU_NONE, MMU_NONE, MMU_R, MMU_NONE,
                MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE, MMU_NONE
                };

            pu_regions[i].data_privileged_perm = privileged_perms[type];
            pu_regions[i].data_user_perm = user_perms[type];
        }
        else
        {
            pu_regions[i].instr_privileged_perm = (type == 1 || type == 2 || type == 3 || type == 5 || type == 6);
            pu_regions[i].instr_user_perm = (type == 2 || type == 3 || type == 6);
        }

        if (pu_regions[i].enabled)
        {
            unmap_pu_region(i);
            remap_pu_region(i);
        }
    }
}

void MMU::set_pu_region(int index, uint32_t value)
{
    printf("[MMU] Set PU%d region: $%08X\n", index, value);
    pu_regions[index].enabled = value & 0x1;
    pu_regions[index].size = 2 << ((value >> 1) & 0x1F);
    pu_regions[index].base = (value >> 12) * 4096;

    printf("Size: $%08X Base: $%08X\n", pu_regions[index].size, pu_regions[index].base);

    unmap_pu_region(index);
    if (pu_regions[index].enabled)
        remap_pu_region(index);
}

void MMU::unmap_pu_region(int index)
{
    uint64_t start = pu_regions[index].base;
    uint64_t end = start + pu_regions[index].size;

    //Higher numbered regions have priority in cases of overlapping
    for (int i = index + 1; i < 8; i++)
    {
        if (!pu_regions[i].enabled)
            continue;

        if (start >= pu_regions[i].base && start < pu_regions[i].base + pu_regions[i].size)
            start = pu_regions[i].base + pu_regions[i].size;

        if (start < pu_regions[i].base && end >= pu_regions[i].base)
            end = pu_regions[i].base;
    }

    start /= 4096;
    end /= 4096;

    while (start < end)
    {
        privileged_mapping[start] = nullptr;
        user_mapping[start] = nullptr;
        start++;
    }
}

void MMU::remap_pu_region(int index)
{
    uint64_t start = pu_regions[index].base;
    uint64_t end = start + pu_regions[index].size;

    //Higher numbered regions have priority in cases of overlapping
    for (int i = index + 1; i < 8; i++)
    {
        if (!pu_regions[i].enabled)
            continue;

        if (start >= pu_regions[i].base && start < pu_regions[i].base + pu_regions[i].size)
            start = pu_regions[i].base + pu_regions[i].size;

        if (start < pu_regions[i].base && end >= pu_regions[i].base)
            end = pu_regions[i].base;
    }

    start /= 4096;
    end /= 4096;

    while (start < end)
    {
        uint64_t addr = (uint64_t)direct_mapping[start];

        //Strip RWX permissions from the address
        addr &= ~(0x7UL << 60UL);

        uint64_t privileged_addr = addr, user_addr = addr;

        privileged_addr |= (uint64_t)(pu_regions[index].instr_privileged_perm) << 60UL;
        privileged_addr |= (uint64_t)(pu_regions[index].data_privileged_perm) << 60UL;

        user_addr |= (uint64_t)(pu_regions[index].instr_user_perm) << 60UL;
        user_addr |= (uint64_t)(pu_regions[index].data_user_perm) << 60UL;

        privileged_mapping[start] = (uint8_t*)privileged_addr;
        user_mapping[start] = (uint8_t*)user_addr;

        start++;
    }
}

void MMU::reload_pu()
{
    for (int i = 0; i < 8; i++)
    {
        unmap_pu_region(i);
        if (pu_regions[i].enabled)
            remap_pu_region(i);
    }
}

MMU_Perm MMU::get_privileged_apx_perms(uint8_t apx)
{
    const static MMU_Perm perms[8] = {MMU_NONE, MMU_RWX, MMU_RWX, MMU_RWX, MMU_NONE, MMU_RX, MMU_RX, MMU_RX};

    return perms[apx & 0x7];
}

MMU_Perm MMU::get_user_apx_perms(uint8_t apx)
{
    const static MMU_Perm perms[8] = {MMU_NONE, MMU_NONE, MMU_RX, MMU_RWX, MMU_NONE, MMU_NONE, MMU_RX, MMU_RX};

    return perms[apx & 0x7];
}
