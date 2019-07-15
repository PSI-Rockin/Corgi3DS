#ifndef ARM_INTERPRET_HPP
#define ARM_INTERPRET_HPP
#include <cstdint>

class ARM_CPU;
class VFP;

namespace ARM_Interpreter
{
    void interpret_arm(ARM_CPU& cpu, uint32_t instr);
    void arm_b(ARM_CPU& cpu, uint32_t instr);
    void arm_bx(ARM_CPU& cpu, uint32_t instr);
    void arm_blx(ARM_CPU& cpu, uint32_t instr);
    void arm_blx_reg(ARM_CPU& cpu, uint32_t instr);
    void arm_clz(ARM_CPU& cpu, uint32_t instr);
    void arm_swp(ARM_CPU& cpu, uint32_t instr);
    void arm_sxtb(ARM_CPU& cpu, uint32_t instr);
    void arm_sxth(ARM_CPU& cpu, uint32_t instr);
    void arm_uxtb(ARM_CPU& cpu, uint32_t instr);
    void arm_uxtab(ARM_CPU& cpu, uint32_t instr);
    void arm_uxth(ARM_CPU& cpu, uint32_t instr);
    void arm_uxtah(ARM_CPU& cpu, uint32_t instr);
    void arm_rev(ARM_CPU& cpu, uint32_t instr);
    void arm_rev16(ARM_CPU& cpu, uint32_t instr);
    void arm_data_processing(ARM_CPU& cpu, uint32_t instr);
    void arm_signed_halfword_multiply(ARM_CPU& cpu, uint32_t instr);
    void arm_uqsub8(ARM_CPU& cpu, uint32_t instr);
    void arm_mul(ARM_CPU& cpu, uint32_t instr);
    void arm_mul_long(ARM_CPU& cpu, uint32_t instr);
    void arm_load_byte(ARM_CPU& cpu, uint32_t instr);
    void arm_store_byte(ARM_CPU& cpu, uint32_t instr);
    void arm_load_word(ARM_CPU& cpu, uint32_t instr);
    void arm_store_word(ARM_CPU& cpu, uint32_t instr);
    void arm_load_halfword(ARM_CPU& cpu, uint32_t instr);
    void arm_store_halfword(ARM_CPU& cpu, uint32_t instr);
    void arm_load_signed_byte(ARM_CPU& cpu, uint32_t instr);
    void arm_load_signed_halfword(ARM_CPU& cpu, uint32_t instr);
    void arm_load_doubleword(ARM_CPU& cpu, uint32_t instr);
    void arm_store_doubleword(ARM_CPU& cpu, uint32_t instr);
    void arm_load_ex_byte(ARM_CPU& cpu, uint32_t instr);
    void arm_store_ex_byte(ARM_CPU& cpu, uint32_t instr);
    void arm_load_ex_halfword(ARM_CPU& cpu, uint32_t instr);
    void arm_store_ex_halfword(ARM_CPU& cpu, uint32_t instr);
    void arm_load_ex_word(ARM_CPU& cpu, uint32_t instr);
    void arm_store_ex_word(ARM_CPU& cpu, uint32_t instr);
    void arm_load_ex_doubleword(ARM_CPU& cpu, uint32_t instr);
    void arm_store_ex_doubleword(ARM_CPU& cpu, uint32_t instr);
    void arm_load_block(ARM_CPU& cpu, uint32_t instr);
    void arm_store_block(ARM_CPU& cpu, uint32_t instr);
    void arm_cop_transfer(ARM_CPU& cpu, uint32_t instr);

    void vfp_single_transfer(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_mov_fpr_gpr(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_mov_sys_reg(ARM_CPU& cpu, VFP& vfp, uint32_t instr);

    void vfp_load_store(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_load_single(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_store_single(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_load_block(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_store_block(ARM_CPU& cpu, VFP& vfp, uint32_t instr);

    void vfp_data_processing(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_mac(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_nmac(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_nmsc(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_mul(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_nmul(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_add(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_sub(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_div(ARM_CPU& cpu, VFP& vfp, uint32_t instr);

    void vfp_data_extended(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_cpy(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_neg(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_abs(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_cmps(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_cmpes(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_sqrt(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_fuito(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_fsito(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_ftoui(ARM_CPU& cpu, VFP& vfp, uint32_t instr);
    void vfp_ftosi(ARM_CPU& cpu, VFP& vfp, uint32_t instr);

    void interpret_thumb(ARM_CPU& cpu, uint16_t instr);
    void thumb_move_shift(ARM_CPU& cpu, uint16_t instr);
    void thumb_add_reg(ARM_CPU& cpu, uint16_t instr);
    void thumb_sub_reg(ARM_CPU& cpu, uint16_t instr);
    void thumb_mov(ARM_CPU& cpu, uint16_t instr);
    void thumb_cmp(ARM_CPU& cpu, uint16_t instr);
    void thumb_add(ARM_CPU& cpu, uint16_t instr);
    void thumb_sub(ARM_CPU& cpu, uint16_t instr);
    void thumb_alu(ARM_CPU& cpu, uint16_t instr);
    void thumb_hi_reg_op(ARM_CPU& cpu, uint16_t instr);
    void thumb_load_imm(ARM_CPU& cpu, uint16_t instr);
    void thumb_store_imm(ARM_CPU& cpu, uint16_t instr);
    void thumb_load_reg(ARM_CPU& cpu, uint16_t instr);
    void thumb_store_reg(ARM_CPU& cpu, uint16_t instr);
    void thumb_load_halfword(ARM_CPU& cpu, uint16_t instr);
    void thumb_store_halfword(ARM_CPU& cpu, uint16_t instr);
    void thumb_load_store_signed(ARM_CPU& cpu, uint16_t instr);
    void thumb_load_block(ARM_CPU& cpu, uint16_t instr);
    void thumb_store_block(ARM_CPU& cpu, uint16_t instr);
    void thumb_push(ARM_CPU& cpu, uint16_t instr);
    void thumb_pop(ARM_CPU& cpu, uint16_t instr);
    void thumb_pc_rel_load(ARM_CPU& cpu, uint16_t instr);
    void thumb_load_addr(ARM_CPU& cpu, uint16_t instr);
    void thumb_sp_rel_load(ARM_CPU& cpu, uint16_t instr);
    void thumb_sp_rel_store(ARM_CPU& cpu, uint16_t instr);
    void thumb_offset_sp(ARM_CPU& cpu, uint16_t instr);
    void thumb_sxth(ARM_CPU& cpu, uint16_t instr);
    void thumb_sxtb(ARM_CPU& cpu, uint16_t instr);
    void thumb_uxth(ARM_CPU& cpu, uint16_t instr);
    void thumb_uxtb(ARM_CPU& cpu, uint16_t instr);
    void thumb_rev(ARM_CPU& cpu, uint16_t instr);
    void thumb_rev16(ARM_CPU& cpu, uint16_t instr);
    void thumb_branch(ARM_CPU& cpu, uint16_t instr);
    void thumb_cond_branch(ARM_CPU& cpu, uint16_t instr);
    void thumb_long_branch_prep(ARM_CPU& cpu, uint16_t instr);
    void thumb_long_branch(ARM_CPU& cpu, uint16_t instr);
    void thumb_long_blx(ARM_CPU& cpu, uint16_t instr);
};

#endif // ARM_INTERPRET_HPP
