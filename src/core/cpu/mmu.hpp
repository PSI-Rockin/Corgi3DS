#ifndef MMU_HPP
#define MMU_HPP
#include <cstdint>

enum MMU_Perm
{
    MMU_NONE,
    MMU_X,
    MMU_W,
    MMU_WX,
    MMU_R,
    MMU_RX,
    MMU_RW,
    MMU_RWX
};

struct PU_Region
{
    uint32_t base, size;
    MMU_Perm data_user_perm, data_privileged_perm;
    bool instr_user_perm, instr_privileged_perm;
    bool enabled;
};

class MMU
{
    private:
        /***
         * MMU-based mappings for user and privileged modes.
         * Every entry is a 4 KB page, totalling to 1 million pages for every mapping.
         * Privileges and other useful modes are set in the upper bits of a 64-bit address, as follows:
         * Bit 63 - When set, indicates the page is not a direct pointer to host memory, but rather a guest address.
         *          The address is masked with 0xFFFFF000 and passed to an emulator read/write method.
         * Bit 62 - When set, indicates the page is readable.
         * Bit 61 - ... writable
         * Bit 60 - ... executable
         * An all-zero entry means the page is unmapped.
        ***/
        uint8_t** user_mapping;
        uint8_t** privileged_mapping;

        //Direct virtual->physical mapping.
        //Follows the same rules as above, except permissions are set to RWX.
        uint8_t** direct_mapping;

        //List of ASIDs - set to 0xFFFF if the page is marked as global
        //This is safe as the maximum ASID is 0xFF
        uint16_t* asid_mapping;

        //Current ASID
        uint8_t asid;

        bool whole_tlb_invalidated;

        uint32_t l1_table_base[2];
        uint64_t l1_table_cutoff;
        uint32_t l1_table_control;
        uint32_t domain_control;

        PU_Region pu_regions[8];

        void unmap_pu_region(int index);
        void remap_pu_region(int index);

        void reload_tlb_section(uint32_t addr);
        void reload_tlb_by_table(int index);
        void remap_mmu_region(uint32_t base, uint32_t size, uint64_t paddr,
                              uint8_t apx, bool exec_never, bool nonglobal);

        MMU_Perm get_user_apx_perms(uint8_t apx);
        MMU_Perm get_privileged_apx_perms(uint8_t apx);
    public:
        MMU();
        ~MMU();

        void reset();
        void add_physical_mapping(uint8_t* mem, uint32_t base, uint32_t size);
        void remove_physical_mapping(uint32_t base, uint32_t size);

        uint8_t** get_user_mapping();
        uint8_t** get_privileged_mapping();
        uint8_t** get_direct_mapping();

        void invalidate_tlb();
        void invalidate_tlb_by_asid(uint8_t value);
        void invalidate_tlb_by_addr(uint32_t value);
        void reload_tlb(uint32_t addr);
        void reload_pu();

        uint32_t get_l1_table_base(int index);
        uint32_t get_l1_table_control();
        uint32_t get_domain_control();
        void set_l1_table_base(int index, uint32_t value);
        void set_l1_table_control(uint32_t value);
        void set_domain_control(uint32_t value);
        void set_asid(uint32_t value);

        void set_pu_permissions_ex(bool is_data, uint32_t value);
        void set_pu_region(int index, uint32_t value);
};

#endif // MMU_HPP
