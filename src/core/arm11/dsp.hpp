#ifndef DSP_HPP
#define DSP_HPP
#include <cstdint>
#include <functional>
#include "dsp_reg.hpp"

struct DSP_APBP
{
    uint16_t cmd[3], reply[3];
    bool cmd_ready[3];
    bool reply_ready[3];

    bool reply_int_enable[3];

    uint16_t dsp_sema_recv, cpu_sema_recv;
    uint16_t dsp_sema_mask, cpu_sema_mask;
};

struct DSP_ST0
{
    bool fr;
};

struct DSP_ST1
{
    uint8_t page;
    uint8_t PS;
    uint8_t ale;
};

struct DSP_ST2
{
    bool s; //shift mode - 0=arith, 1=logic
};

struct DSP_STT0
{
    bool flm;
    bool fvl;
    bool fe;
    bool fc;
    bool fv;
    bool fn;
    bool fm;
    bool fz;
    bool fc1;
};

struct DSP_STT2
{
    bool int_pending[3];
    bool vectored_int_pending;
    uint8_t pcmhi;
    uint8_t bcn;
    bool lp;
};

struct DSP_MOD0
{
    bool sat;
    bool sata;
    uint8_t hwm;
    uint8_t PS1;
};

struct DSP_MOD1
{
    bool stp16;
    bool cmd;
    bool epi;
    bool epj;
};

struct DSP_MOD2
{
    bool m[8];
    bool br[8];
};

struct DSP_MOD3
{
    bool nmi_ctx_switch;
    bool int_ctx_switch[3];

    bool master_int_enable;
    bool int_enable[3];
    bool vectored_int_enable;

    bool ccnta;
    bool reverse_stack_order;
    bool crep;
};

struct DSP_BKREP_ELEMENT
{
    uint16_t lc;
    uint32_t start;
    uint32_t end;
};

struct DSP_AR
{
    uint8_t step[2];
    uint8_t offset[2];
    uint8_t rn[2];
};

struct DSP_TIMER
{
    uint32_t restart_value;
    uint32_t counter;
};

struct DSP_MIU
{
    uint16_t mmio_base;

    uint8_t xpage, ypage, zpage;
    uint16_t x_size[2], y_size[2];

    bool zsp;
    bool page_mode;
};

struct DSP_DMA
{
    //FIFO/pipe stuff
    uint32_t arm_addr;
    uint8_t fifo_len;
    uint8_t cur_fifo_len;
    bool fifo_started;
    bool auto_inc;
    uint8_t mem_type;

    //DMA proper
    uint16_t chan_enable;
    uint8_t channel;
    
    uint32_t src_addr[8];
    uint32_t dest_addr[8];
    uint16_t size[3][8];
    uint16_t src_step[3][8];
    uint16_t dest_step[3][8];
};

struct DSP_ICU
{
    uint16_t int_pending;

    uint16_t int_connection[3];
    uint16_t vectored_int_connection;
    uint16_t int_mode;
    uint16_t int_polarity;

    bool vector_ctx_switch[16];
    uint32_t vector_addr[16];
};

class DSP
{
    private:
        std::function<void()> send_arm_interrupt;
        bool halted;
        uint32_t pc;

        uint64_t a0, a1, b0, b1;
        uint64_t a1s, b1s;

        uint16_t sp;
        uint16_t repc, repcs;
        bool rep;

        uint16_t x[2], y[2];
        uint32_t p[2];
        uint16_t pe[2];
        uint16_t ps[2];

        uint16_t r[8];
        uint16_t stepi, stepj;
        uint16_t modi, modj;
        uint32_t stepi0, stepj0;

        uint16_t r0b, r1b, r4b, r7b;
        uint16_t stepib, stepjb;
        uint16_t modib, modjb;
        uint32_t stepi0b, stepj0b;

        DSP_AR ar[2], ars[2];
        DSP_AR arp[4], arps[4];

        uint16_t mixp;

        DSP_TIMER timers[2];
        DSP_MIU miu;
        DSP_APBP apbp;
        DSP_DMA dma;
        DSP_ICU icu;

        DSP_ST0 st0, st0s;
        DSP_ST1 st1, st1s;
        DSP_ST2 st2, st2s;
        DSP_STT0 stt0, stt0s;
        DSP_STT2 stt2, stt2s;
        DSP_MOD0 mod0, mod0s;
        DSP_MOD1 mod1, mod1s;
        DSP_MOD2 mod2, mod2s;
        DSP_MOD3 mod3, mod3s;

        uint16_t sv;

        //Although only 3 levels are permitted for bkrep, we keep 4 because (re)store accesses bcn + 1
        DSP_BKREP_ELEMENT bkrep_stack[4];
        uint32_t rep_new_pc;

        uint8_t* dsp_mem;

        bool running;

        uint16_t get_ar(int index);
        uint16_t get_arp(int index);
        uint32_t convert_addr(uint16_t addr);
        uint64_t trunc_to_40(uint64_t value);
        uint64_t saturate(uint64_t value);
        void set_acc(DSP_REG acc, uint64_t value);
        void set_ar(int index, uint16_t value);
        void set_arp(int index, uint16_t value);

        void assert_dsp_irq(int id);
        void int_check();
        void do_irq(uint32_t addr, uint8_t type);

        void apbp_send_cmd(int index, uint16_t value);
    public:
        DSP();

        void reset(uint8_t* dsp_mem);
        void set_cpu_interrupt_sender(std::function<void()> func);
        void run(int cycles);
        void halt();
        void unhalt();

        uint16_t read16(uint32_t addr);
        void write16(uint32_t addr, uint16_t value);

        void print_state();

        uint16_t fetch_code_word();

        uint16_t read_program_word(uint32_t addr);
        uint16_t read_data_word(uint16_t addr);
        uint16_t read_from_page(uint8_t imm);
        uint16_t read_data_r7s(int16_t imm);
        void write_data_word(uint16_t addr, uint16_t value);
        void write_to_page(uint8_t imm, uint16_t value);
        void write_data_r7s(int16_t imm, uint16_t value);

        bool meets_condition(uint8_t cond);

        uint64_t get_add_sub_result(uint64_t a, uint64_t b, bool is_sub);
        uint64_t get_acc(DSP_REG acc);
        uint32_t get_product_no_shift(int index);
        uint64_t get_product(int index);

        uint32_t get_pc();
        uint16_t get_sv();
        uint16_t get_reg16(DSP_REG reg, bool mov_saturate);
        uint64_t get_saturated_acc(DSP_REG reg);
        uint16_t get_rn(int index);
        uint16_t get_x(int index);
        uint16_t get_y(int index);
        uint16_t get_stepi0();
        uint16_t get_stepj0();
        uint16_t get_repc();
        uint16_t get_mixp();
        void set_reg16(DSP_REG reg, uint16_t value);
        void set_acc_lo(DSP_REG acc, uint16_t value);
        void set_acc_hi(DSP_REG acc, uint16_t value);
        void set_acc_flags(uint64_t value);
        void set_acc_and_flag(DSP_REG acc, uint64_t value);
        void saturate_acc_with_flag(DSP_REG acc, uint64_t value);
        void set_product(int index, uint32_t value);

        void set_pc(uint32_t addr);
        void branch(int offset);

        void set_page(uint16_t value);
        void set_ps01(uint16_t value);
        void set_stepi(uint16_t value);
        void set_stepj(uint16_t value);
        void set_stepi0(uint16_t value);
        void set_stepj0(uint16_t value);
        void set_sv(uint16_t value);
        void set_x(int index, uint16_t value);
        void set_y(int index, uint16_t value);
        void set_repc(uint16_t value);
        void set_mixp(uint16_t value);

        void push16(uint16_t value);
        uint16_t pop16();
        void push_pc();
        void pop_pc();

        void save_shadows();
        void swap_shadows();
        void swap_all_ararps();
        void restore_shadows();

        void save_context();
        void restore_context();
        void banke(uint8_t flags);

        void break_();
        void repeat(uint16_t lc);
        void block_repeat(uint16_t lc, uint32_t end_addr);
        uint16_t restore_block_repeat(uint16_t addr);
        uint16_t store_block_repeat(uint16_t addr);

        void shift_reg_40(uint64_t value, DSP_REG dest, uint16_t shift);
        void multiply(uint32_t unit, bool x_sign, bool y_sign);

        uint8_t get_arrn_unit(uint8_t value);
        uint8_t get_arstep(uint8_t value);
        uint8_t get_aroffset(uint8_t value);
        uint16_t rn_addr(uint8_t rn, uint16_t value);
        uint16_t rn_and_modify(uint8_t rn, uint8_t step, bool dmod);
        uint16_t rn_addr_and_modify(uint8_t rn, uint8_t step, bool dmod);
        uint16_t step_addr(uint8_t rn, uint16_t value, uint8_t step, bool dmod);
        uint16_t offset_addr(uint8_t rn, uint16_t addr, uint8_t offset, bool dmod);

        void set_fz(bool f);
        void set_fc(bool f);
        void set_fm(bool f);
        void check_fr(uint8_t rn);

        void set_master_int_enable(bool ie);
};

inline uint32_t DSP::get_pc()
{
    return pc;
}

inline uint16_t DSP::get_sv()
{
    return sv;
}

inline uint16_t DSP::get_rn(int index)
{
    return r[index];
}

inline uint16_t DSP::get_x(int index)
{
    return x[index];
}

inline uint16_t DSP::get_y(int index)
{
    return y[index];
}

inline uint16_t DSP::get_stepi0()
{
    return stepi0;
}

inline uint16_t DSP::get_stepj0()
{
    return stepj0;
}

inline uint16_t DSP::get_repc()
{
    return repc;
}

inline uint16_t DSP::get_mixp()
{
    return mixp;
}

inline void DSP::set_pc(uint32_t addr)
{
    pc = addr & 0x3FFFF;
}

inline void DSP::branch(int offset)
{
    pc += offset;
}

inline void DSP::set_page(uint16_t value)
{
    st1.page = value;
}

inline void DSP::set_ps01(uint16_t value)
{
    st1.PS = value & 0x3;
    mod0.PS1 = (value >> 2) & 0x3;
}

inline void DSP::set_stepi(uint16_t value)
{
    stepi = value & 0x7F;
}

inline void DSP::set_stepj(uint16_t value)
{
    stepj = value & 0x7F;
}

inline void DSP::set_stepi0(uint16_t value)
{
    stepi0 = value;
}

inline void DSP::set_stepj0(uint16_t value)
{
    stepj0 = value;
}

inline void DSP::set_sv(uint16_t value)
{
    sv = value;
}

inline void DSP::set_x(int index, uint16_t value)
{
    x[index] = value;
}

inline void DSP::set_y(int index, uint16_t value)
{
    y[index] = value;
}

inline void DSP::set_repc(uint16_t value)
{
    repc = value;
}

inline void DSP::set_mixp(uint16_t value)
{
    mixp = value;
}

inline void DSP::set_fz(bool f)
{
    stt0.fz = f;
}

inline void DSP::set_fc(bool f)
{
    stt0.fc = f;
}

inline void DSP::set_fm(bool f)
{
    stt0.fm = f;
}

inline void DSP::check_fr(uint8_t rn)
{
    st0.fr = r[rn] == 0;
}

inline void DSP::set_master_int_enable(bool ie)
{
    mod3.master_int_enable = ie;
}

#endif // DSP_HPP
