#ifndef ARM_HPP
#define ARM_HPP
#include <cstdint>
#include <string>
#include "cp15.hpp"

#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

#define CARRY_ADD(a, b)  ((0xFFFFFFFF-a) < b)
#define CARRY_SUB(a, b)  (a >= b)

#define ADD_OVERFLOW(a, b, result) ((!(((a) ^ (b)) & 0x80000000)) && (((a) ^ (result)) & 0x80000000))
#define SUB_OVERFLOW(a, b, result) (((a) ^ (b)) & 0x80000000) && (((a) ^ (result)) & 0x80000000)

enum PSR_MODE
{
    PSR_USER = 0x10,
    PSR_FIQ = 0x11,
    PSR_IRQ = 0x12,
    PSR_SUPERVISOR = 0x13,
    PSR_ABORT = 0x17,
    PSR_UNDEFINED = 0x1B,
    PSR_SYSTEM = 0x1F
};

struct PSR_Flags
{
    PSR_MODE mode;
    bool thumb;
    bool fiq_disable, irq_disable;

    bool negative;
    bool zero;
    bool carry;
    bool overflow;
    bool q_overflow;
    bool ge[4];

    uint32_t get();
    void set(uint32_t value);
};

class Emulator;
class VFP;

class ARM_CPU
{
    private:
        Emulator* e;
        int id;
        uint32_t gpr[16];
        bool prefetch_abort_occurred;
        bool halted;
        bool waiting_for_event;
        bool can_disassemble;
        bool int_pending;
        bool event_pending;
        int cycles_ran;
        uint64_t local_exclusive_start, local_exclusive_end;

        CP15* cp15;
        VFP* vfp;

        uint32_t fiq_regs[5];
        uint32_t SP_und, SP_irq, SP_svc, SP_fiq, SP_abt;
        uint32_t LR_und, LR_irq, LR_svc, LR_fiq, LR_abt;

        PSR_Flags CPSR, SPSR[0x20];

        uint8_t** tlb_map;
        uint8_t* instr_ptr;

        void fetch_new_instr_ptr(uint32_t addr);
    public:
        static uint64_t global_exclusive_start[4], global_exclusive_end[4];
        ARM_CPU(Emulator* e, int id, CP15* cp15, VFP* vfp);

        static std::string get_reg_name(int id);

        void reset();
        void run(int cycles);
        void print_state();
        int get_id();
        int get_cycles_ran();
        void inc_cycle_count(int delta);

        bool is_halted();
        uint32_t get_PC();
        PSR_Flags* get_CPSR();

        uint8_t read8(uint32_t addr);
        uint16_t read16(uint32_t addr);
        uint32_t read32(uint32_t addr);
        uint64_t read64(uint32_t addr);

        void write8(uint32_t addr, uint8_t value);
        void write16(uint32_t addr, uint16_t value);
        void write32(uint32_t addr, uint32_t value);
        void write64(uint32_t addr, uint64_t value);

        bool has_exclusive(uint32_t addr);
        void set_exclusive(uint32_t addr, uint32_t size);
        void clear_global_exclusives(uint32_t addr);
        void clear_exclusive();

        uint32_t get_register(int id);
        void set_register(int id, uint32_t value);

        void data_abort(uint32_t addr, bool is_write);
        void prefetch_abort(uint32_t addr);
        void swi();
        void und();
        void int_check();
        void set_int_signal(bool pending);
        void halt();
        void unhalt();
        void wfe();
        void sev();
        void send_event(int id);
        void set_disassembly(bool dis);

        void jp(uint32_t addr, bool change_thumb_state);
        void set_zero_neg_flags(uint32_t value);
        void set_zero(bool flag);
        void set_neg(bool flag);
        void set_CV_add_flags(uint32_t a, uint32_t b, uint32_t result);
        void set_CV_sub_flags(uint32_t a, uint32_t b, uint32_t result);
        void update_reg_mode(PSR_MODE mode);
        void spsr_to_cpsr();
        bool meets_condition(int cond);

        void andd(int destination, int source, int operand, bool set_condition_codes);
        void orr(int destination, int source, int operand, bool set_condition_codes);
        void eor(int destination, int source, int operand, bool set_condition_codes);
        void add(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes);
        void sub(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes);
        void adc(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes);
        void sbc(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes);
        void cmp(uint32_t x, uint32_t y);
        void cmn(uint32_t x, uint32_t y);
        void tst(uint32_t x, uint32_t y);
        void teq(uint32_t x, uint32_t y);
        void mov(uint32_t destination, uint32_t operand, bool alter_flags);
        void mul(uint32_t destination, uint32_t source, uint32_t operand, bool set_condition_codes);
        void bic(uint32_t destination, uint32_t source, uint32_t operand, bool alter_flags);
        void mvn(uint32_t destination, uint32_t operand, bool alter_flags);
        void mrs(uint32_t instr);
        void msr(uint32_t instr);
        void cps(uint32_t instr);
        void srs(uint32_t instr);
        void rfe(uint32_t instr);

        void vfp_single_transfer(uint32_t instr);
        void vfp_load_store(uint32_t instr);
        void vfp_data_processing(uint32_t instr);

        uint32_t mrc(int coprocessor_id, int operation_mode, int CP_reg,
                     int coprocessor_info, int coprocessor_operand);
        void mcr(int coprocessor_id, int operation_mode, int CP_reg,
                     int coprocessor_info, int coprocessor_operand, uint32_t value);

        uint32_t lsl(uint32_t value, int shift, bool alter_flags);
        uint32_t lsr(uint32_t value, int shift, bool alter_flags);
        uint32_t lsr_32(uint32_t value, bool alter_flags);
        uint32_t asr(uint32_t value, int shift, bool alter_flags);
        uint32_t asr_32(uint32_t value, bool alter_flags);
        uint32_t rrx(uint32_t value, bool alter_flags);
        uint32_t rotr32(uint32_t n, unsigned int c, bool alter_flags);
};

inline bool ARM_CPU::is_halted()
{
    return halted;
}

inline int ARM_CPU::get_id()
{
    return id;
}

inline int ARM_CPU::get_cycles_ran()
{
    return cycles_ran;
}

inline void ARM_CPU::inc_cycle_count(int delta)
{
    cycles_ran += delta;
}

inline uint32_t ARM_CPU::get_register(int id)
{
    return gpr[id];
}

inline void ARM_CPU::set_register(int id, uint32_t value)
{
    if (can_disassemble)
        printf("[ARM%d] Set reg%d: $%08X\n", this->id, id, value);
    gpr[id] = value;
}

inline uint32_t ARM_CPU::get_PC()
{
    return gpr[15];
}

inline PSR_Flags* ARM_CPU::get_CPSR()
{
    return &CPSR;
}

inline void ARM_CPU::set_zero(bool flag)
{
    CPSR.zero = flag;
}

inline void ARM_CPU::set_neg(bool flag)
{
    CPSR.negative = flag;
}

inline void ARM_CPU::set_disassembly(bool dis)
{
    can_disassemble = dis;
}

#endif // ARM_HPP
