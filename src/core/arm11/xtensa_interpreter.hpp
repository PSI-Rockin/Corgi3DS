#ifndef XTENSA_INTERPRETER_HPP
#define XTENSA_INTERPRETER_HPP
#include <cstdint>

class Xtensa;

namespace Xtensa_Interpreter
{

void interpret(Xtensa& cpu, uint32_t instr);
void l32r(Xtensa& cpu, uint32_t instr);
void l32i_n(Xtensa& cpu, uint32_t instr);
void s32i_n(Xtensa& cpu, uint32_t instr);
void add_n(Xtensa& cpu, uint32_t instr);
void addi_n(Xtensa& cpu, uint32_t instr);

void op0_qrst(Xtensa& cpu, uint32_t instr);
void extui(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst0(Xtensa& cpu, uint32_t instr);
void and_(Xtensa& cpu, uint32_t instr);
void or_(Xtensa& cpu, uint32_t instr);
void addx2(Xtensa& cpu, uint32_t instr);
void addx8(Xtensa& cpu, uint32_t instr);
void sub(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst0_st0(Xtensa& cpu, uint32_t instr);
void rsil(Xtensa& cpu, uint32_t instr);
void waiti(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst0_st0_snm0(Xtensa& cpu, uint32_t instr);
void jx(Xtensa& cpu, uint32_t instr);
void callx4(Xtensa& cpu, uint32_t instr);
void callx8(Xtensa& cpu, uint32_t instr);
void callx12(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst0_st0_rfei(Xtensa& cpu, uint32_t instr);
void rfi(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst0_rt0(Xtensa& cpu, uint32_t instr);
void neg(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst0_st1(Xtensa& cpu, uint32_t instr);
void ssl(Xtensa& cpu, uint32_t instr);
void nsau(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst1(Xtensa& cpu, uint32_t instr);
void slli(Xtensa& cpu, uint32_t instr);
void srli(Xtensa& cpu, uint32_t instr);
void xsr(Xtensa& cpu, uint32_t instr);
void sll(Xtensa& cpu, uint32_t instr);

void op0_qrst_rst3(Xtensa& cpu, uint32_t instr);
void rsr(Xtensa& cpu, uint32_t instr);
void wsr(Xtensa& cpu, uint32_t instr);
void moveqz(Xtensa& cpu, uint32_t instr);
void movnez(Xtensa& cpu, uint32_t instr);

void op0_lsai(Xtensa& cpu, uint32_t instr);
void l8i(Xtensa& cpu, uint32_t instr);
void l32i(Xtensa& cpu, uint32_t instr);
void s8i(Xtensa& cpu, uint32_t instr);
void s32i(Xtensa& cpu, uint32_t instr);
void movi(Xtensa& cpu, uint32_t instr);
void addi(Xtensa& cpu, uint32_t instr);

void op0_calln(Xtensa& cpu, uint32_t instr);
void call0(Xtensa& cpu, uint32_t instr);
void call4(Xtensa& cpu, uint32_t instr);
void call8(Xtensa& cpu, uint32_t instr);
void call12(Xtensa& cpu, uint32_t instr);

void op0_si(Xtensa& cpu, uint32_t instr);
void j(Xtensa& cpu, uint32_t instr);
void beqz(Xtensa& cpu, uint32_t instr);
void beqi(Xtensa& cpu, uint32_t instr);
void entry(Xtensa& cpu, uint32_t instr);
void bnez(Xtensa& cpu, uint32_t instr);
void bnei(Xtensa& cpu, uint32_t instr);
void bltui(Xtensa& cpu, uint32_t instr);
void bgei(Xtensa& cpu, uint32_t instr);
void bgeui(Xtensa& cpu, uint32_t instr);

void op0_si_b1(Xtensa& cpu, uint32_t instr);
void loopnez(Xtensa& cpu, uint32_t instr);
void loopgtz(Xtensa& cpu, uint32_t instr);

void op0_b(Xtensa& cpu, uint32_t instr);
void bnone(Xtensa& cpu, uint32_t instr);
void beq(Xtensa& cpu, uint32_t instr);
void bltu(Xtensa& cpu, uint32_t instr);
void bbci(Xtensa& cpu, uint32_t instr);
void bne(Xtensa& cpu, uint32_t instr);
void bgeu(Xtensa& cpu, uint32_t instr);

void op0_st2(Xtensa& cpu, uint32_t instr);
void movi_n(Xtensa& cpu, uint32_t instr);
void beqz_n(Xtensa& cpu, uint32_t instr);
void bnez_n(Xtensa& cpu, uint32_t instr);

void op0_st3(Xtensa& cpu, uint32_t instr);
void mov(Xtensa& cpu, uint32_t instr);

void op0_st3_s3(Xtensa& cpu, uint32_t instr);

};

#endif // XTENSA_INTERPRETER_HPP
