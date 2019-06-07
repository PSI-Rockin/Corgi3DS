#ifndef CP15_HPP
#define CP15_HPP
#include <cstdint>

class ARM_CPU;
class MMU;

class CP15
{
    private:
        int id;
        ARM_CPU* cpu;
        MMU* mmu;

        bool mmu_enabled;
        bool high_exception_vector;
    public:
        uint8_t ITCM[1024 * 32], DTCM[1024 * 16];

        uint32_t itcm_size;
        uint32_t dtcm_base, dtcm_size;
        CP15(int id, ARM_CPU* cpu, MMU* mmu);

        void reset(bool has_tcm);

        uint8_t** get_tlb_mapping();

        bool has_high_exceptions();

        uint32_t mrc(int operation_mode, int CP_reg,
                     int coprocessor_info, int coprocessor_operand);
        void mcr(int operation_mode, int CP_reg,
                 int coprocessor_info, int coprocessor_operand, uint32_t value);
};

#endif // CP15_HPP
