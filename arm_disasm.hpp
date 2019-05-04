#ifndef ARM_DISASM_HPP
#define ARM_DISASM_HPP

#include <string>

enum ARM_INSTR
{
    ARM_UNDEFINED,
    ARM_B,
    ARM_BL,
    ARM_BX,
    ARM_BLX,
    ARM_CLZ,
    ARM_DATA_PROCESSING,
    ARM_MULTIPLY,
    ARM_MULTIPLY_LONG,
    ARM_SWAP,
    ARM_SIGNED_HALFWORD_MULTIPLY,
    ARM_LOAD_HALFWORD,
    ARM_STORE_HALFWORD,
    ARM_LOAD_SIGNED_BYTE,
    ARM_LOAD_DOUBLEWORD,
    ARM_LOAD_SIGNED_HALFWORD,
    ARM_STORE_DOUBLEWORD,

    ARM_STORE_WORD,
    ARM_LOAD_WORD,
    ARM_STORE_BYTE,
    ARM_LOAD_BYTE,
    ARM_STORE_BLOCK,
    ARM_LOAD_BLOCK,

    ARM_COP_REG_TRANSFER,
    ARM_COP_DATA_OP,

    ARM_WFI
};

enum THUMB_INSTR
{
    THUMB_UNDEFINED,
    THUMB_MOV_SHIFT,
    THUMB_ADD_REG,
    THUMB_SUB_REG,
    THUMB_MOV_IMM,
    THUMB_CMP_IMM,
    THUMB_ADD_IMM,
    THUMB_SUB_IMM,
    THUMB_ALU_OP,
    THUMB_HI_REG_OP,
    THUMB_PC_REL_LOAD,
    THUMB_STORE_REG_OFFSET,
    THUMB_LOAD_REG_OFFSET,
    THUMB_LOAD_STORE_SIGN_HALFWORD,
    THUMB_STORE_HALFWORD,
    THUMB_LOAD_HALFWORD,
    THUMB_STORE_IMM_OFFSET,
    THUMB_LOAD_IMM_OFFSET,
    THUMB_SP_REL_STORE,
    THUMB_SP_REL_LOAD,
    THUMB_OFFSET_SP,
    THUMB_SXTH,
    THUMB_SXTB,
    THUMB_UXTH,
    THUMB_UXTB,
    THUMB_LOAD_ADDRESS,
    THUMB_POP,
    THUMB_PUSH,
    THUMB_STORE_MULTIPLE,
    THUMB_LOAD_MULTIPLE,
    THUMB_BRANCH,
    THUMB_COND_BRANCH,
    THUMB_LONG_BRANCH_PREP,
    THUMB_LONG_BRANCH,
    THUMB_LONG_BLX,
    THUMB_SWI
};

ARM_INSTR decode_arm(uint32_t instr);
THUMB_INSTR decode_thumb(uint16_t instr);

class ARM_CPU;

namespace ARM_Disasm
{
    std::string cond_name(int cond);
    std::string disasm_arm(ARM_CPU& cpu, uint32_t instr);
    std::string arm_b(ARM_CPU& cpu, uint32_t instr);
    std::string arm_bx(uint32_t instr);
    std::string arm_data_processing(uint32_t instr);
    std::string arm_mul(uint32_t instr);
    std::string arm_mul_long(uint32_t instr);
    std::string arm_mrs(uint32_t instr);
    std::string arm_msr(uint32_t instr);
    std::string arm_load_store(ARM_CPU& cpu, uint32_t instr);
    std::string arm_load_store_block(uint32_t instr);
    std::string arm_cop_transfer(uint32_t instr);

    std::string disasm_thumb(ARM_CPU& cpu, uint16_t instr);
    std::string thumb_move_shift(ARM_CPU& cpu, uint16_t instr);
    std::string thumb_op_imm(uint16_t instr);
    std::string thumb_op_reg(uint16_t instr);
    std::string thumb_alu(uint16_t instr);
    std::string thumb_hi_reg_op(uint16_t instr);
    std::string thumb_load_store_imm(uint16_t instr);
    std::string thumb_load_store_reg(uint16_t instr);
    std::string thumb_load_store_halfword(uint16_t instr);
    std::string thumb_load_store_sign_halfword(uint16_t instr);
    std::string thumb_load_store_block(uint16_t instr);
    std::string thumb_push_pop(uint16_t instr);
    std::string thumb_pc_rel_load(ARM_CPU& cpu, uint16_t instr);
    std::string thumb_load_addr(ARM_CPU& cpu, uint16_t instr);
    std::string thumb_sp_rel_load_store(ARM_CPU& cpu, uint16_t instr);
    std::string thumb_offset_sp(uint16_t instr);
    std::string thumb_extend_op(uint16_t instr);
    std::string thumb_branch(ARM_CPU& cpu, uint16_t instr);
    std::string thumb_cond_branch(ARM_CPU& cpu, uint16_t instr);
    std::string thumb_long_branch(ARM_CPU& cpu, uint16_t instr);
};

#endif // ARM_DISASM_HPP
