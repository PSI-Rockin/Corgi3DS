#ifndef ARM_INTERPRET_HPP
#define ARM_INTERPRET_HPP
#include <cstdint>

class ARM_CPU;

namespace ARM_Interpreter
{
    void interpret_arm(ARM_CPU& cpu, uint32_t instr);
    void arm_b(ARM_CPU& cpu, uint32_t instr);
    void arm_bx(ARM_CPU& cpu, uint32_t instr);
    void arm_blx(ARM_CPU& cpu, uint32_t instr);
    void arm_blx_reg(ARM_CPU& cpu, uint32_t instr);
    void arm_clz(ARM_CPU& cpu, uint32_t instr);
    void arm_data_processing(ARM_CPU& cpu, uint32_t instr);
    void arm_mul(ARM_CPU& cpu, uint32_t instr);
    void arm_mul_long(ARM_CPU& cpu, uint32_t instr);
    void arm_load_byte(ARM_CPU& cpu, uint32_t instr);
    void arm_store_byte(ARM_CPU& cpu, uint32_t instr);
    void arm_load_word(ARM_CPU& cpu, uint32_t instr);
    void arm_store_word(ARM_CPU& cpu, uint32_t instr);
    void arm_load_halfword(ARM_CPU& cpu, uint32_t instr);
    void arm_store_halfword(ARM_CPU& cpu, uint32_t instr);
    void arm_load_block(ARM_CPU& cpu, uint32_t instr);
    void arm_store_block(ARM_CPU& cpu, uint32_t instr);
    void arm_cop_transfer(ARM_CPU& cpu, uint32_t instr);

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
    void thumb_branch(ARM_CPU& cpu, uint16_t instr);
    void thumb_cond_branch(ARM_CPU& cpu, uint16_t instr);
    void thumb_long_branch_prep(ARM_CPU& cpu, uint16_t instr);
    void thumb_long_branch(ARM_CPU& cpu, uint16_t instr);
    void thumb_long_blx(ARM_CPU& cpu, uint16_t instr);
};

#endif // ARM_INTERPRET_HPP
