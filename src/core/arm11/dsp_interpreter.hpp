#ifndef DSP_INTERPRETER_HPP
#define DSP_INTERPRETER_HPP
#include <cstdint>
#include "dsp_reg.hpp"

class DSP;

enum DSP_INSTR
{
    DSP_NOP,
    DSP_SWAP,

    DSP_ALM_MEMIMM8,
    DSP_ALM_RN_STEP,
    DSP_ALM_REG,

    DSP_ALU_MEMIMM16,
    DSP_ALU_MEMR7IMM16,
    DSP_ALU_IMM16,
    DSP_ALU_IMM8,
    DSP_ALU_MEMR7IMM7S,

    DSP_OR_1,
    DSP_OR_2,

    DSP_ALB_MEMIMM8,
    DSP_ALB_RN_STEP,
    DSP_ALB_REG,

    DSP_ADD_AB_BX,
    DSP_ADD_BX_AX,
    DSP_ADD_PX_BX,

    DSP_SUB_AB_BX,
    DSP_SUB_BX_AX,

    DSP_SET_STTMOD,
    DSP_RST_STTMOD,

    DSP_APP_ZR,
    DSP_APP_AC_ADD_PP_PA,
    DSP_APP_AC_ADD_PA_PA,

    DSP_MUL_ARSTEP_IMM16,
    DSP_MUL_Y0_ARSTEP,
    DSP_MUL_Y0_REG,
    DSP_MUL_R45_ARSTEP,

    DSP_MPYI,

    DSP_MODA4,
    DSP_MODA3,

    DSP_BKREP_IMM8,
    DSP_BKREP_REG,
    DSP_BKREP_R6,
    DSP_BKREPRST_MEMSP,
    DSP_BKREPSTO_MEMSP,

    DSP_BANKE,

    DSP_BR,
    DSP_BRR,

    DSP_BREAK_,

    DSP_CALL,
    DSP_CALLA_AX,
    DSP_CALLR,

    DSP_CNTX_S,
    DSP_CNTX_R,

    DSP_RET,
    DSP_RETI,
    DSP_RETIC,
    DSP_RETS,

    DSP_PUSH_IMM16,
    DSP_PUSH_REG,
    DSP_PUSH_ABE,
    DSP_PUSH_ARARPSTTMOD,
    DSP_PUSH_PX,
    DSP_PUSH_R6,
    DSP_PUSH_REPC,
    DSP_PUSH_X0,
    DSP_PUSH_X1,
    DSP_PUSH_Y1,
    DSP_PUSHA_AX,
    DSP_PUSHA_BX,

    DSP_POP_REG,
    DSP_POP_ABE,
    DSP_POP_ARARPSTTMOD,
    DSP_POP_PX,
    DSP_POP_R6,
    DSP_POP_REPC,
    DSP_POP_X0,
    DSP_POP_X1,
    DSP_POP_Y1,
    DSP_POPA,

    DSP_REP_IMM,
    DSP_REP_REG,

    DSP_SHFC,
    DSP_SHFI,

    DSP_TSTB_MEMIMM8,
    DSP_TSTB_RN_STEP,
    DSP_TSTB_REG,

    DSP_AND_,

    DSP_DINT,
    DSP_EINT,

    DSP_EXP_REG,

    DSP_MODR,
    DSP_MODR_I2,
    DSP_MODR_D2,
    DSP_MODR_EEMOD,

    DSP_MOV_AB_AB,
    DSP_MOV_Y1,

    DSP_MOV_ABLH_MEMIMM8,
    DSP_MOV_AXL_MEMIMM16,
    DSP_MOV_AXL_MEMR7IMM7S,

    DSP_MOV_MEMIMM8_AB,
    DSP_MOV_MEMIMM8_ABLH,
    DSP_MOV_MEMIMM8_RNOLD,

    DSP_MOV_MEMIMM16_AX,
    DSP_MOV_IMM16_BX,
    DSP_MOV_IMM16_REG,
    DSP_MOV_IMM8S_RNOLD,
    DSP_MOV_SV_MEMIMM8,
    DSP_MOV_SV_IMM8S,
    DSP_MOV_IMM8_AXL,
    DSP_MOV_RN_STEP_BX,
    DSP_MOV_RN_STEP_REG,

    DSP_MOV_MEMR7IMM16_AX,
    DSP_MOV_MEMR7IMM7S_AX,
    DSP_MOV_REG_BX,
    DSP_MOV_MIXP_REG,
    DSP_MOV_RNOLD_MEMIMM8,
    DSP_MOV_REG_MIXP,
    DSP_MOV_REG_REG,
    DSP_MOV_REG_RN_STEP,
    DSP_MOV_SV_TO,

    DSP_LOAD_PAGE,
    DSP_LOAD_PS01,

    DSP_MOV_STTMOD,
    DSP_MOVP_REG,
    DSP_MOV_ABL_STTMOD,
    DSP_MOV_STTMOD_ABL,
    DSP_MOV_ARARP,
    DSP_MOV_STEPI,
    DSP_MOV_STEPJ,
    DSP_MOV_R6,
    DSP_MOV_STEPI0,
    DSP_MOV_STEPJ0,

    DSP_MOV_A0H_STEPI0,
    DSP_MOV_A0H_STEPJ0,
    DSP_MOV_STEPI0_A0H,
    DSP_MOV_STEPJ0_A0H,

    DSP_MOV_ABL_ARARP,
    DSP_MOV_ARARP_ABL,

    DSP_MOV_AX_PC,

    DSP_MOV_AB_P0,

    DSP_MOV2_PX_ARSTEP,
    DSP_MOV2_ARSTEP_PX,
    DSP_MOVA_AB_ARSTEP,
    DSP_MOVA_ARSTEP_AB,

    DSP_MOV_R6_REG,
    DSP_MOV_REG_R6,

    DSP_MOVS_MEMIMM8_AB,
    DSP_MOVS_RN_STEP_AB,
    DSP_MOVS_REG_AB,
    DSP_MOVSI,

    DSP_MOV2_ABH_M,

    DSP_CLRP0,
    DSP_CLRP1,
    DSP_CLRP,

    DSP_MAX_GT,
    DSP_MIN_LT,

    DSP_MMA_EMOD_EMOD_SYXX_XYXX_AC,
    DSP_MMA_EMOD_EMOD_SYSX_SYSX_ZR,
    DSP_MMA_EMOD_EMOD_SYSX_SYSX_AC_2,

    DSP_MMA_MY_MY,

    DSP_UNDEFINED
};

namespace DSP_Interpreter
{
    bool no_match(uint16_t instr, int start, int size, uint16_t value);
    DSP_INSTR decode(uint16_t instr);
    DSP_REG get_register(uint8_t reg);
    DSP_REG get_mov_from_p_reg(uint8_t reg);
    DSP_REG get_ax_reg(uint8_t ax);
    DSP_REG get_axl_reg(uint8_t axl);
    DSP_REG get_bx_reg(uint8_t bx);
    DSP_REG get_ab_reg(uint8_t ab);
    DSP_REG get_abe_reg(uint8_t abe);
    DSP_REG get_abh_reg(uint8_t abh);
    DSP_REG get_abl_reg(uint8_t abl);
    DSP_REG get_ablh_reg(uint8_t ablh);
    DSP_REG get_sttmod_reg(uint8_t sttmod);
    DSP_REG get_ararp_reg(uint8_t ararp);
    DSP_REG get_ararpsttmod_reg(uint8_t ararpsttmod);
    DSP_REG get_rnold(uint8_t rnold);
    DSP_REG get_counter_acc(DSP_REG acc);

    void interpret(DSP& dsp, uint16_t instr);

    void swap(DSP& dsp, uint16_t instr);

    bool is_alb_modifying(uint8_t op);
    uint16_t do_alb_op(DSP& dsp, uint16_t a, uint16_t b, uint8_t op);

    uint64_t signextend_alm(uint16_t value, uint8_t op);
    void do_alm_op(DSP& dsp, DSP_REG acc, uint64_t value, uint8_t op);
    void do_mul3_op(DSP& dsp, DSP_REG acc, uint8_t op);

    void alm_memimm8(DSP& dsp, uint16_t instr);
    void alm_rn_step(DSP& dsp, uint16_t instr);
    void alm_reg(DSP& dsp, uint16_t instr);

    void alu_memimm16(DSP& dsp, uint16_t instr);
    void alu_memr7imm16(DSP& dsp, uint16_t instr);
    void alu_imm16(DSP& dsp, uint16_t instr);
    void alu_imm8(DSP& dsp, uint16_t instr);
    void alu_memr7imm7s(DSP& dsp, uint16_t instr);

    void or_1(DSP& dsp, uint16_t instr);
    void or_2(DSP& dsp, uint16_t instr);

    void alb_memimm8(DSP& dsp, uint16_t instr);
    void alb_rn_step(DSP& dsp, uint16_t instr);
    void alb_reg(DSP& dsp, uint16_t instr);

    void add_ab_bx(DSP& dsp, uint16_t instr);
    void add_bx_ax(DSP& dsp, uint16_t instr);
    void add_px_bx(DSP& dsp, uint16_t instr);

    void sub_ab_bx(DSP& dsp, uint16_t instr);
    void sub_bx_ax(DSP& dsp, uint16_t instr);

    void set_sttmod(DSP& dsp, uint16_t instr);
    void rst_sttmod(DSP& dsp, uint16_t instr);

    void app_zr(DSP& dsp, uint16_t instr);
    void app_ac_add_pp_pa(DSP& dsp, uint16_t instr);
    void app_ac_add_pa_pa(DSP& dsp, uint16_t instr);

    void mul_arstep_imm16(DSP& dsp, uint16_t instr);
    void mul_y0_arstep(DSP& dsp, uint16_t instr);
    void mul_y0_reg(DSP& dsp, uint16_t instr);
    void mul_r45_arstep(DSP& dsp, uint16_t instr);

    void mpyi(DSP& dsp, uint16_t instr);

    void moda4(DSP& dsp, uint16_t instr);
    void moda3(DSP& dsp, uint16_t instr);

    void bkrep_imm8(DSP& dsp, uint16_t instr);
    void bkrep_reg(DSP& dsp, uint16_t instr);
    void bkrep_r6(DSP& dsp, uint16_t instr);
    void bkreprst_memsp(DSP& dsp, uint16_t instr);
    void bkrepsto_memsp(DSP& dsp, uint16_t instr);

    void banke(DSP& dsp, uint16_t instr);

    void br(DSP& dsp, uint16_t instr);
    void brr(DSP& dsp, uint16_t instr);

    void break_(DSP& dsp, uint16_t instr);

    void call(DSP& dsp, uint16_t instr);
    void calla_ax(DSP& dsp, uint16_t instr);
    void callr(DSP& dsp, uint16_t instr);

    void cntx_s(DSP& dsp, uint16_t instr);
    void cntx_r(DSP& dsp, uint16_t instr);

    void ret(DSP& dsp, uint16_t instr);
    void reti(DSP& dsp, uint16_t instr);
    void retic(DSP& dsp, uint16_t instr);
    void rets(DSP& dsp, uint16_t instr);

    void push_imm16(DSP& dsp, uint16_t instr);
    void push_reg(DSP& dsp, uint16_t instr);
    void push_abe(DSP& dsp, uint16_t instr);
    void push_ararpsttmod(DSP& dsp, uint16_t instr);
    void push_px(DSP& dsp, uint16_t instr);
    void push_r6(DSP& dsp, uint16_t instr);
    void push_repc(DSP& dsp, uint16_t instr);
    void push_x0(DSP& dsp, uint16_t instr);
    void push_x1(DSP& dsp, uint16_t instr);
    void push_y1(DSP& dsp, uint16_t instr);
    void pusha_ax(DSP& dsp, uint16_t instr);
    void pusha_bx(DSP& dsp, uint16_t instr);

    void pop_reg(DSP& dsp, uint16_t instr);
    void pop_abe(DSP& dsp, uint16_t instr);
    void pop_ararpsttmod(DSP& dsp, uint16_t instr);
    void pop_px(DSP& dsp, uint16_t instr);
    void pop_r6(DSP& dsp, uint16_t instr);
    void pop_repc(DSP& dsp, uint16_t instr);
    void pop_x0(DSP& dsp, uint16_t instr);
    void pop_x1(DSP& dsp, uint16_t instr);
    void pop_y1(DSP& dsp, uint16_t instr);
    void popa(DSP& dsp, uint16_t instr);

    void rep_imm(DSP& dsp, uint16_t instr);
    void rep_reg(DSP& dsp, uint16_t instr);

    void shfc(DSP& dsp, uint16_t instr);
    void shfi(DSP& dsp, uint16_t instr);

    void tstb_memimm8(DSP& dsp, uint16_t instr);
    void tstb_rn_step(DSP& dsp, uint16_t instr);
    void tstb_reg(DSP& dsp, uint16_t instr);

    void and_(DSP& dsp, uint16_t instr);

    void dint(DSP& dsp, uint16_t instr);
    void eint(DSP& dsp, uint16_t instr);

    void exp_reg(DSP& dsp, uint16_t instr);

    void modr(DSP& dsp, uint16_t instr);
    void modr_i2(DSP& dsp, uint16_t instr);
    void modr_d2(DSP& dsp, uint16_t instr);
    void modr_eemod(DSP& dsp, uint16_t instr);

    void mov_ab_ab(DSP& dsp, uint16_t instr);
    void mov_y1(DSP& dsp, uint16_t instr);

    void mov_ablh_memimm8(DSP& dsp, uint16_t instr);
    void mov_axl_memimm16(DSP& dsp, uint16_t instr);
    void mov_axl_memr7imm7s(DSP& dsp, uint16_t instr);

    void mov_memimm8_ab(DSP& dsp, uint16_t instr);
    void mov_memimm8_ablh(DSP& dsp, uint16_t instr);
    void mov_memimm8_rnold(DSP& dsp, uint16_t instr);

    void mov_memimm16_ax(DSP& dsp, uint16_t instr);
    void mov_imm16_bx(DSP& dsp, uint16_t instr);
    void mov_imm16_reg(DSP& dsp, uint16_t instr);
    void mov_imm8s_rnold(DSP& dsp, uint16_t instr);
    void mov_sv_memimm8(DSP& dsp, uint16_t instr);
    void mov_sv_imm8s(DSP& dsp, uint16_t instr);
    void mov_imm8_axl(DSP& dsp, uint16_t instr);
    void mov_rn_step_bx(DSP& dsp, uint16_t instr);
    void mov_rn_step_reg(DSP& dsp, uint16_t instr);

    void mov_memr7imm16_ax(DSP& dsp, uint16_t instr);
    void mov_memr7imm7s_ax(DSP& dsp, uint16_t instr);
    void mov_reg_bx(DSP& dsp, uint16_t instr);
    void mov_mixp_reg(DSP& dsp, uint16_t instr);
    void mov_rnold_memimm8(DSP& dsp, uint16_t instr);
    void mov_reg_mixp(DSP& dsp, uint16_t instr);
    void mov_reg_reg(DSP& dsp, uint16_t instr);
    void mov_reg_rn_step(DSP& dsp, uint16_t instr);
    void mov_sv_to(DSP& dsp, uint16_t instr);

    void load_page(DSP& dsp, uint16_t instr);
    void load_ps01(DSP& dsp, uint16_t instr);
    void mov_sttmod(DSP& dsp, uint16_t instr);
    void movp_reg(DSP& dsp, uint16_t instr);
    void mov_abl_sttmod(DSP& dsp, uint16_t instr);
    void mov_sttmod_abl(DSP& dsp, uint16_t instr);
    void mov_ararp(DSP& dsp, uint16_t instr);
    void mov_r6(DSP& dsp, uint16_t instr);
    void mov_stepi(DSP& dsp, uint16_t instr);
    void mov_stepj(DSP& dsp, uint16_t instr);
    void mov_stepi0(DSP& dsp, uint16_t instr);
    void mov_stepj0(DSP& dsp, uint16_t instr);

    void mov_a0h_stepi0(DSP& dsp, uint16_t instr);
    void mov_a0h_stepj0(DSP& dsp, uint16_t instr);
    void mov_stepi0_a0h(DSP& dsp, uint16_t instr);
    void mov_stepj0_a0h(DSP& dsp, uint16_t instr);

    void mov_abl_ararp(DSP& dsp, uint16_t instr);
    void mov_ararp_abl(DSP& dsp, uint16_t instr);

    void mov_ax_pc(DSP& dsp, uint16_t instr);

    void mov_ab_p0(DSP& dsp, uint16_t instr);

    void mov2_px_arstep(DSP& dsp, uint16_t instr);
    void mov2_arstep_px(DSP& dsp, uint16_t instr);
    void mova_ab_arstep(DSP& dsp, uint16_t instr);
    void mova_arstep_ab(DSP& dsp, uint16_t instr);

    void mov_r6_reg(DSP& dsp, uint16_t instr);
    void mov_reg_r6(DSP& dsp, uint16_t instr);

    void movs_memimm8_ab(DSP& dsp, uint16_t instr);
    void movs_rn_step_ab(DSP& dsp, uint16_t instr);
    void movs_reg_ab(DSP& dsp, uint16_t instr);
    void movsi(DSP& dsp, uint16_t instr);

    void mov2_abh_m(DSP& dsp, uint16_t instr);

    void clrp0(DSP& dsp, uint16_t instr);
    void clrp1(DSP& dsp, uint16_t instr);
    void clrp(DSP& dsp, uint16_t instr);

    void max_gt(DSP& dsp, uint16_t instr);
    void min_lt(DSP& dsp, uint16_t instr);

    void mma_emod_emod_syxx_xyxx_ac(DSP& dsp, uint16_t instr);
    void mma_emod_emod_sysx_sysx_zr(DSP& dsp, uint16_t instr);
    void mma_emod_emod_sysx_sysx_ac_2(DSP& dsp, uint16_t instr);

    void mma_my_my(DSP& dsp, uint16_t instr);
};

#endif // DSP_INTERPRETER_HPP
