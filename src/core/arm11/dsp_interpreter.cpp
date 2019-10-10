#include "dsp.hpp"
#include "dsp_interpreter.hpp"
#include "signextend.hpp"
#include "../common/common.hpp"

namespace DSP_Interpreter
{

bool no_match(uint16_t instr, int start, int size, uint16_t value)
{
    return ((instr >> start) & ((1 << size) - 1)) != value;
}

DSP_INSTR decode(uint16_t instr)
{
    if (instr == 0)
        return DSP_NOP;

    if ((instr & ~0x000F) == 0x4980)
        return DSP_SWAP;

    if ((instr & ~0x1FFF) == 0xA000)
        return DSP_ALM_MEMIMM8;

    if ((instr & ~0x1F1F) == 0x8080)
        return DSP_ALM_RN_STEP;

    if ((instr & ~0x1F1F) == 0x80A0)
        return DSP_ALM_REG;

    if ((instr & ~0x0107) == 0xD4F8)
    {
        if (no_match(instr, 0, 3, 4) && no_match(instr, 0, 3, 5))
            return DSP_ALU_MEMIMM16;
    }

    if ((instr & ~0x0107) == 0xD4D8)
    {
        if (no_match(instr, 0, 3, 4) && no_match(instr, 0, 3, 5))
            return DSP_ALU_MEMR7IMM16;
    }

    if ((instr & ~0x0F00) == 0x80C0)
    {
        if (no_match(instr, 9, 3, 4) && no_match(instr, 9, 3, 5))
            return DSP_ALU_IMM16;
    }

    if ((instr & ~0x0FFF) == 0xC000)
    {
        if (no_match(instr, 9, 3, 4) && no_match(instr, 9, 3, 5))
            return DSP_ALU_IMM8;
    }

    if ((instr & ~0x0F7F) == 0x4000)
    {
        if (no_match(instr, 9, 3, 4) && no_match(instr, 9, 3, 5))
            return DSP_ALU_MEMR7IMM7S;
    }

    if ((instr & ~0x0C60) == 0xD291)
        return DSP_OR_1;

    if ((instr & ~0x0103) == 0xD4A4)
        return DSP_OR_2;

    if ((instr & ~0x0EFF) == 0xE100)
        return DSP_ALB_MEMIMM8;

    if ((instr & ~0x0E1F) == 0x80E0)
        return DSP_ALB_RN_STEP;

    if ((instr & ~0x0E1F) == 0x81E0)
        return DSP_ALB_REG;

    if ((instr & ~0x0C01) == 0xD2DA)
        return DSP_ADD_AB_BX;

    if ((instr & ~0x0003) == 0x5DF0)
        return DSP_ADD_BX_AX;

    if ((instr & ~0x0001) == 0xD782)
        return DSP_ADD_P1_AX;

    if ((instr & ~0x0003) == 0x5DF8)
        return DSP_ADD_PX_BX;

    if ((instr & ~0x0118) == 0x8A61)
        return DSP_SUB_AB_BX;

    if ((instr & ~0x0018) == 0x8861)
        return DSP_SUB_BX_AX;

    if ((instr & ~0x0007) == 0x43C8)
        return DSP_SET_STTMOD;

    if ((instr & ~0x0007) == 0x4388)
        return DSP_RST_STTMOD;

    if ((instr & ~0x000F) == 0x5DC0)
        return DSP_APP_ZR;

    if ((instr & ~0x000C) == 0x4590)
        return DSP_APP_AC_ADD_PP_PP;

    if ((instr & ~0x000C) == 0x4592)
        return DSP_APP_AC_ADD_PP_PA;

    if ((instr & ~0x000C) == 0x4593)
        return DSP_APP_AC_ADD_PA_PA;

    if ((instr & ~0x0F1F) == 0x8000)
        return DSP_MUL_ARSTEP_IMM16;

    if ((instr & ~0x0F1F) == 0x8020)
        return DSP_MUL_Y0_ARSTEP;

    if ((instr & ~0x0F1F) == 0x8040)
        return DSP_MUL_Y0_REG;

    if ((instr & ~0x0F7F) == 0xD000)
        return DSP_MUL_R45_ARSTEP;

    if ((instr & ~0x0EFF) == 0xE000)
        return DSP_MUL_Y0_MEMIMM8;

    if ((instr & ~0x00FF) == 0x0800)
        return DSP_MPYI;

    if ((instr & ~0x10FF) == 0x6700)
    {
        if (no_match(instr, 4, 4, 7))
            return DSP_MODA4;
    }

    if ((instr & ~0x107F) == 0x6F00)
        return DSP_MODA3;

    if ((instr & ~0x00FF) == 0x5C00)
        return DSP_BKREP_IMM8;

    if ((instr & ~0x007F) == 0x5D00)
        return DSP_BKREP_REG;

    if ((instr & ~0x0003) == 0x8FDC)
        return DSP_BKREP_R6;

    if (instr == 0x5F48)
        return DSP_BKREPRST_MEMSP;

    if (instr == 0x9468)
        return DSP_BKREPSTO_MEMSP;

    if ((instr & ~0x003F) == 0x4B80)
        return DSP_BANKE;

    if ((instr & ~0x003F) == 0x4180)
        return DSP_BR;

    if ((instr & ~0x07FF) == 0x5000)
        return DSP_BRR;

    if (instr == 0xD3C0)
        return DSP_BREAK_;

    if ((instr & ~0x003F) == 0x41C0)
        return DSP_CALL;

    if ((instr & ~0x0010) == 0xD381)
        return DSP_CALLA_AX;

    if ((instr & ~0x07FF) == 0x1000)
        return DSP_CALLR;

    if (instr == 0xD380)
        return DSP_CNTX_S;

    if (instr == 0xD390)
        return DSP_CNTX_R;

    if ((instr & ~0x000F) == 0x4580)
        return DSP_RET;

    if ((instr & ~0x000F) == 0x45C0)
        return DSP_RETI;

    if ((instr & ~0x000F) == 0x45D0)
        return DSP_RETIC;

    if ((instr & ~0x00FF) == 0x0900)
        return DSP_RETS;

    if (instr == 0x5F40)
        return DSP_PUSH_IMM16;

    if ((instr & ~0x001F) == 0x5E40)
        return DSP_PUSH_REG;

    if ((instr & ~0x0006) == 0xD7C8)
        return DSP_PUSH_ABE;

    if ((instr & ~0x000F) == 0xD3D0)
        return DSP_PUSH_ARARPSTTMOD;

    if ((instr & ~0x0002) == 0xD78C)
        return DSP_PUSH_PX;

    if (instr == 0xD4D7)
        return DSP_PUSH_R6;

    if (instr == 0xD7F8)
        return DSP_PUSH_REPC;

    if (instr == 0xD4D4)
        return DSP_PUSH_X0;

    if (instr == 0xD4D5)
        return DSP_PUSH_X1;

    if (instr == 0xD4D6)
        return DSP_PUSH_Y1;

    if ((instr & ~0x0040) == 0x4384)
        return DSP_PUSHA_AX;

    if ((instr & ~0x0002) == 0xD788)
        return DSP_PUSHA_BX;

    if ((instr & ~0x001F) == 0x5E60)
        return DSP_POP_REG;

    if ((instr & ~0x0003) == 0x47B4)
        return DSP_POP_ABE;

    if ((instr & ~0x0F00) == 0x80C7)
        return DSP_POP_ARARPSTTMOD;

    if ((instr & ~0x0001) == 0xD496)
        return DSP_POP_PX;

    if (instr == 0x0024)
        return DSP_POP_R6;

    if (instr == 0xD7F0)
        return DSP_POP_REPC;

    if (instr == 0xD494)
        return DSP_POP_X0;

    if (instr == 0xD495)
        return DSP_POP_X1;

    if (instr == 0x0004)
        return DSP_POP_Y1;

    if ((instr & ~0x0003) == 0x47B0)
        return DSP_POPA;

    if ((instr & ~0x00FF) == 0x0C00)
        return DSP_REP_IMM;

    if ((instr & ~0x001F) == 0x0D00)
        return DSP_REP_REG;

    if ((instr & ~0x0C6F) == 0xD280)
        return DSP_SHFC;

    if ((instr & ~0x003F & ~0x0180 & ~0x0C00) == 0x9240)
        return DSP_SHFI;

    if ((instr & ~0x0FFF) == 0xF000)
        return DSP_TSTB_MEMIMM8;

    if ((instr & ~0x0F1F) == 0x9020)
        return DSP_TSTB_RN_STEP;

    if ((instr & ~0x0F1F) == 0x9000)
        return DSP_TSTB_REG;

    if ((instr & ~0x100F) == 0x6770)
        return DSP_AND_;

    if (instr == 0x43C0)
        return DSP_DINT;

    if (instr == 0x4380)
        return DSP_EINT;

    if ((instr & ~0x001F) == 0x9440)
        return DSP_EXP_REG;

    if ((instr & ~0x001F) == 0x0080)
        return DSP_MODR;

    if ((instr & ~0x0007) == 0x4990)
        return DSP_MODR_I2;

    if ((instr & ~0x0007) == 0x5DA0)
        return DSP_MODR_D2;

    if ((instr & ~0x0C63) == 0xD294)
        return DSP_MODR_EEMOD;

    if ((instr & ~0x0C60) == 0xD290)
        return DSP_MOV_AB_AB;

    if ((instr & ~0x0003) == 0xD384)
        return DSP_MOV_Y1;

    if ((instr & ~0x0EFF) == 0x3000)
        return DSP_MOV_ABLH_MEMIMM8;

    if ((instr & ~0x0100) == 0xD4BC)
        return DSP_MOV_AXL_MEMIMM16;

    if ((instr & ~0x017F) == 0xDC80)
        return DSP_MOV_AXL_MEMR7IMM7S;

    if ((instr & ~0x18FF) == 0x6100)
        return DSP_MOV_MEMIMM8_AB;

    if ((instr & ~0x1CFF) == 0x6200)
        return DSP_MOV_MEMIMM8_ABLH;

    if ((instr & ~0x1CFF) == 0x6000)
        return DSP_MOV_MEMIMM8_RNOLD;

    if ((instr & ~0x0100) == 0xD4B8)
        return DSP_MOV_MEMIMM16_AX;

    if ((instr & ~0x0100) == 0x5E20)
        return DSP_MOV_IMM16_BX;

    if ((instr & ~0x001F) == 0x5E00)
        return DSP_MOV_IMM16_REG;

    if ((instr & ~0x1CFF) == 0x2300)
        return DSP_MOV_IMM8S_RNOLD;

    if ((instr & ~0x00FF) == 0x6D00)
        return DSP_MOV_SV_MEMIMM8;

    if ((instr & ~0x00FF) == 0x0500)
        return DSP_MOV_SV_IMM8S;

    if ((instr & ~0x10FF) == 0x2100)
        return DSP_MOV_IMM8_AXL;

    if ((instr & ~0x011F) == 0x98C0)
        return DSP_MOV_RN_STEP_BX;

    if ((instr & ~0x03FF) == 0x1C00)
        return DSP_MOV_RN_STEP_REG;

    if ((instr & ~0x017F) == 0xD880)
        return DSP_MOV_MEMR7IMM7S_AX;

    if ((instr & ~0x0100) == 0xD498)
        return DSP_MOV_MEMR7IMM16_AX;

    if ((instr & ~0x003F) == 0x5EC0)
        return DSP_MOV_REG_BX;

    if ((instr & ~0x001F) == 0x47C0)
        return DSP_MOV_MIXP_REG;

    if ((instr & ~0x0EFF) == 0x2000)
        return DSP_MOV_RNOLD_MEMIMM8;

    if ((instr & ~0x001F) == 0x5E80)
        return DSP_MOV_REG_MIXP;

    if ((instr & ~0x03FF) == 0x5800)
    {
        if (no_match(instr, 0, 5, 24) && no_match(instr, 0, 5, 25))
            return DSP_MOV_REG_REG;
    }

    if ((instr & ~0x03FF) == 0x1800)
    {
        if (no_match(instr, 5, 5, 24) && no_match(instr, 5, 5, 25))
            return DSP_MOV_REG_RN_STEP;
    }

    if ((instr & ~0x00FF) == 0x7D00)
        return DSP_MOV_SV_TO;

    if ((instr & ~0x00FF) == 0x0400)
        return DSP_LOAD_PAGE;

    if ((instr & ~0x000F) == 0x0010)
        return DSP_LOAD_PS01;

    if ((instr & ~0x0007) == 0x0030)
        return DSP_MOV_STTMOD;

    if ((instr & ~0x003F) == 0x0D40)
        return DSP_MOVP_REG;

    if ((instr & ~0x001F) == 0x9C60)
        return DSP_MOV_ABL_STTMOD;

    if ((instr & ~0x0C07) == 0xD2F8)
        return DSP_MOV_STTMOD_ABL;

    if ((instr & ~0x0007) == 0x0008)
        return DSP_MOV_ARARP;

    if ((instr & ~0x007F) == 0xDB80)
        return DSP_MOV_STEPI;

    if ((instr & ~0x007F) == 0xDF80)
        return DSP_MOV_STEPJ;

    if (instr == 0x0023)
        return DSP_MOV_R6;

    if (instr == 0x8971)
        return DSP_MOV_STEPI0;

    if (instr == 0x8979)
        return DSP_MOV_STEPJ0;

    if (instr == 0xD49B)
        return DSP_MOV_A0H_STEPI0;

    if (instr == 0xD59B)
        return DSP_MOV_A0H_STEPJ0;

    if (instr == 0xD482)
        return DSP_MOV_STEPI0_A0H;

    if (instr == 0xD582)
        return DSP_MOV_STEPJ0_A0H;

    if ((instr & ~0x001F) == 0x9540)
        return DSP_MOV_ABL_ARARP;

    if ((instr & ~0x001F) == 0x9560)
        return DSP_MOV_ARARP_ABL;

    if ((instr & ~0x0100) == 0x886B)
        return DSP_MOV_AX_PC;

    if ((instr & ~0x0003) == 0x8FD4)
        return DSP_MOV_AB_P0;

    if ((instr & ~0x0003) == 0x8FD8)
        return DSP_MOV_P1_AB;

    if ((instr & ~0x030E) == 0x88D0)
        return DSP_MOV2_PX_ARSTEP;

    if ((instr & ~0x0C61) == 0xD292)
        return DSP_MOV2_ARSTEP_PX;

    if ((instr & ~0x003F) == 0x4DC0)
        return DSP_MOVA_AB_ARSTEP;

    if ((instr & ~0x003F) == 0x4BC0)
        return DSP_MOVA_ARSTEP_AB;

    if ((instr & ~0x001F) == 0x5F00)
        return DSP_MOV_R6_REG;

    if ((instr & ~0x001F) == 0x5F60)
        return DSP_MOV_REG_R6;

    if ((instr & ~0x18FF) == 0x6300)
        return DSP_MOVS_MEMIMM8_AB;

    if ((instr & ~0x007F) == 0x0180)
        return DSP_MOVS_RN_STEP_AB;

    if ((instr & ~0x007F) == 0x0100)
        return DSP_MOVS_REG_AB;

    if ((instr & ~0x0E7F) == 0x4080)
        return DSP_MOVSI;

    if ((instr & ~0x003F) == 0x9D40)
        return DSP_MOV2_ABH_M;

    if ((instr & ~0x007F) == 0x4900)
        return DSP_EXCHANGE_JAI;

    if ((instr & ~0x0030) == 0x49C0)
        return DSP_LIM;

    if (instr == 0x5DFE)
        return DSP_CLRP0;

    if (instr == 0x5DFD)
        return DSP_CLRP1;

    if (instr == 0x5DFF)
        return DSP_CLRP;

    if ((instr & ~0x0118) == 0x8660)
        return DSP_MAX_GT;

    if ((instr & ~0x0118) == 0x8A60)
        return DSP_MIN_LT;

    if ((instr & ~0x01FF) == 0xCA00)
        return DSP_MMA_EMOD_EMOD_SYXX_XYXX_AC;

    if ((instr & ~0x0D07) == 0x82C8)
        return DSP_MMA_EMOD_EMOD_SYSX_SYSX_ZR;

    if ((instr & ~0x01FF) == 0xC800)
        return DSP_MMA_EMOD_EMOD_SYSX_SYSX_AC_2;

    if ((instr & ~0x011F) == 0x94E0)
        return DSP_MMA_MY_MY;

    return DSP_UNDEFINED;
}

DSP_REG get_register(uint8_t reg)
{
    switch (reg)
    {
        case 0x00:
            return DSP_REG_R0;
        case 0x01:
            return DSP_REG_R1;
        case 0x02:
            return DSP_REG_R2;
        case 0x03:
            return DSP_REG_R3;
        case 0x04:
            return DSP_REG_R4;
        case 0x05:
            return DSP_REG_R5;
        case 0x06:
            return DSP_REG_R7; //R7, not R6!
        case 0x07:
            return DSP_REG_Y0;
        case 0x08:
            return DSP_REG_ST0;
        case 0x09:
            return DSP_REG_ST1;
        case 0x0A:
            return DSP_REG_ST2;
        case 0x0B:
            return DSP_REG_P;
        case 0x0C:
            return DSP_REG_PC;
        case 0x0D:
            return DSP_REG_SP;
        case 0x0E:
            return DSP_REG_CFGI;
        case 0x0F:
            return DSP_REG_CFGJ;
        case 0x10:
            return DSP_REG_B0h;
        case 0x11:
            return DSP_REG_B1h;
        case 0x12:
            return DSP_REG_B0l;
        case 0x13:
            return DSP_REG_B1l;
        case 0x18:
            return DSP_REG_A0;
        case 0x19:
            return DSP_REG_A1;
        case 0x1A:
            return DSP_REG_A0l;
        case 0x1B:
            return DSP_REG_A1l;
        case 0x1C:
            return DSP_REG_A0h;
        case 0x1D:
            return DSP_REG_A1h;
        case 0x1E:
            return DSP_REG_LC;
        case 0x1F:
            return DSP_REG_SV;
        default:
            EmuException::die("[DSP_Interpreter] Unrecognized reg format $%02X in get_register", reg);
            return DSP_REG_UNK;
    }
}

DSP_REG get_mov_from_p_reg(uint8_t reg)
{
    return (reg & 0x1) ? DSP_REG_A1 : DSP_REG_A0;
}

DSP_REG get_ax_reg(uint8_t ax)
{
    if (ax == 0)
        return DSP_REG_A0;
    else if (ax == 1)
        return DSP_REG_A1;
    return DSP_REG_UNK;
}

DSP_REG get_bx_reg(uint8_t bx)
{
    if (bx == 0)
        return DSP_REG_B0;
    else if (bx == 1)
        return DSP_REG_B1;
    return DSP_REG_UNK;
}

DSP_REG get_axl_reg(uint8_t axl)
{
    if (axl == 0)
        return DSP_REG_A0l;
    else if (axl == 1)
        return DSP_REG_A1l;
    return DSP_REG_UNK;
}

DSP_REG get_axh_reg(uint8_t axh)
{
    if (axh == 0)
        return DSP_REG_A0h;
    else if (axh == 1)
        return DSP_REG_A1h;
    return DSP_REG_UNK;
}

DSP_REG get_ab_reg(uint8_t ab)
{
    switch (ab)
    {
        case 0x0:
            return DSP_REG_B0;
        case 0x1:
            return DSP_REG_B1;
        case 0x2:
            return DSP_REG_A0;
        case 0x3:
            return DSP_REG_A1;
        default:
            return DSP_REG_UNK;
    }
}

DSP_REG get_abe_reg(uint8_t abe)
{
    switch (abe)
    {
        case 0x0:
            return DSP_REG_B0e;
        case 0x1:
            return DSP_REG_B1e;
        case 0x2:
            return DSP_REG_A0e;
        case 0x3:
            return DSP_REG_A1e;
        default:
            return DSP_REG_UNK;
    }
}

DSP_REG get_abh_reg(uint8_t abh)
{
    switch (abh)
    {
        case 0x0:
            return DSP_REG_B0h;
        case 0x1:
            return DSP_REG_B1h;
        case 0x2:
            return DSP_REG_A0h;
        case 0x3:
            return DSP_REG_A1h;
        default:
            return DSP_REG_UNK;
    }
}

DSP_REG get_abl_reg(uint8_t abl)
{
    switch (abl)
    {
        case 0x0:
            return DSP_REG_B0l;
        case 0x1:
            return DSP_REG_B1l;
        case 0x2:
            return DSP_REG_A0l;
        case 0x3:
            return DSP_REG_A1l;
        default:
            return DSP_REG_UNK;
    }
}

DSP_REG get_ablh_reg(uint8_t ablh)
{
    switch (ablh)
    {
        case 0x0:
            return DSP_REG_B0l;
        case 0x1:
            return DSP_REG_B0h;
        case 0x2:
            return DSP_REG_B1l;
        case 0x3:
            return DSP_REG_B1h;
        case 0x4:
            return DSP_REG_A0l;
        case 0x5:
            return DSP_REG_A0h;
        case 0x6:
            return DSP_REG_A1l;
        case 0x7:
            return DSP_REG_A1h;
        default:
            return DSP_REG_UNK;
    }
}

DSP_REG get_sttmod_reg(uint8_t sttmod)
{
    switch (sttmod)
    {
        case 0x0:
            return DSP_REG_STT0;
        case 0x1:
            return DSP_REG_STT1;
        case 0x2:
            return DSP_REG_STT2;
        case 0x4:
            return DSP_REG_MOD0;
        case 0x5:
            return DSP_REG_MOD1;
        case 0x6:
            return DSP_REG_MOD2;
        case 0x7:
            return DSP_REG_MOD3;
        default:
            return DSP_REG_UNK;
    }
}

DSP_REG get_ararp_reg(uint8_t ararp)
{
    switch (ararp)
    {
        case 0x0:
            return DSP_REG_AR0;
        case 0x1:
            return DSP_REG_AR1;
        case 0x2:
            return DSP_REG_ARP0;
        case 0x3:
            return DSP_REG_ARP1;
        case 0x4:
            return DSP_REG_ARP2;
        case 0x5:
            return DSP_REG_ARP3;
        default:
            return DSP_REG_UNK;
    }
}

DSP_REG get_ararpsttmod_reg(uint8_t ararpsttmod)
{
    if (ararpsttmod < 8)
        return get_ararp_reg(ararpsttmod);
    return get_sttmod_reg(ararpsttmod - 8);
}

DSP_REG get_rnold(uint8_t rnold)
{
    return get_register(rnold & 0x7);
}

DSP_REG get_counter_acc(DSP_REG acc)
{
    switch (acc)
    {
        case DSP_REG_A0:
            return DSP_REG_A1;
        case DSP_REG_A1:
            return DSP_REG_A0;
        default:
            EmuException::die("[DSP_Interpreter] Unrecognized acc %d in get_counter_acc", acc);
            return DSP_REG_UNK;
    }
}

void interpret(DSP &dsp, uint16_t instr)
{
    switch (decode(instr))
    {
        case DSP_NOP:
            break;
        case DSP_SWAP:
            swap(dsp, instr);
            break;
        case DSP_ALM_MEMIMM8:
            alm_memimm8(dsp, instr);
            break;
        case DSP_ALM_RN_STEP:
            alm_rn_step(dsp, instr);
            break;
        case DSP_ALM_REG:
            alm_reg(dsp, instr);
            break;
        case DSP_ALU_MEMIMM16:
            alu_memimm16(dsp, instr);
            break;
        case DSP_ALU_MEMR7IMM16:
            alu_memr7imm16(dsp, instr);
            break;
        case DSP_ALU_IMM16:
            alu_imm16(dsp, instr);
            break;
        case DSP_ALU_IMM8:
            alu_imm8(dsp, instr);
            break;
        case DSP_ALU_MEMR7IMM7S:
            alu_memr7imm7s(dsp, instr);
            break;
        case DSP_OR_1:
            or_1(dsp, instr);
            break;
        case DSP_OR_2:
            or_2(dsp, instr);
            break;
        case DSP_ALB_MEMIMM8:
            alb_memimm8(dsp, instr);
            break;
        case DSP_ALB_RN_STEP:
            alb_rn_step(dsp, instr);
            break;
        case DSP_ALB_REG:
            alb_reg(dsp, instr);
            break;
        case DSP_ADD_AB_BX:
            add_ab_bx(dsp, instr);
            break;
        case DSP_ADD_BX_AX:
            add_bx_ax(dsp, instr);
            break;
        case DSP_ADD_P1_AX:
            add_p1_ax(dsp, instr);
            break;
        case DSP_ADD_PX_BX:
            add_px_bx(dsp, instr);
            break;
        case DSP_SUB_AB_BX:
            sub_ab_bx(dsp, instr);
            break;
        case DSP_SUB_BX_AX:
            sub_bx_ax(dsp, instr);
            break;
        case DSP_SET_STTMOD:
            set_sttmod(dsp, instr);
            break;
        case DSP_RST_STTMOD:
            rst_sttmod(dsp, instr);
            break;
        case DSP_APP_ZR:
            app_zr(dsp, instr);
            break;
        case DSP_APP_AC_ADD_PP_PP:
            app_ac_add_pp_pp(dsp, instr);
            break;
        case DSP_APP_AC_ADD_PP_PA:
            app_ac_add_pp_pa(dsp, instr);
            break;
        case DSP_APP_AC_ADD_PA_PA:
            app_ac_add_pa_pa(dsp, instr);
            break;
        case DSP_MUL_ARSTEP_IMM16:
            mul_arstep_imm16(dsp, instr);
            break;
        case DSP_MUL_Y0_ARSTEP:
            mul_y0_arstep(dsp, instr);
            break;
        case DSP_MUL_Y0_REG:
            mul_y0_reg(dsp, instr);
            break;
        case DSP_MUL_R45_ARSTEP:
            mul_r45_arstep(dsp, instr);
            break;
        case DSP_MUL_Y0_MEMIMM8:
            mul_y0_memimm8(dsp, instr);
            break;
        case DSP_MPYI:
            mpyi(dsp, instr);
            break;
        case DSP_MODA4:
            moda4(dsp, instr);
            break;
        case DSP_MODA3:
            moda3(dsp, instr);
            break;
        case DSP_BKREP_IMM8:
            bkrep_imm8(dsp, instr);
            break;
        case DSP_BKREP_REG:
            bkrep_reg(dsp, instr);
            break;
        case DSP_BKREP_R6:
            bkrep_r6(dsp, instr);
            break;
        case DSP_BKREPRST_MEMSP:
            bkreprst_memsp(dsp, instr);
            break;
        case DSP_BKREPSTO_MEMSP:
            bkrepsto_memsp(dsp, instr);
            break;
        case DSP_BANKE:
            banke(dsp, instr);
            break;
        case DSP_BR:
            br(dsp, instr);
            break;
        case DSP_BRR:
            brr(dsp, instr);
            break;
        case DSP_BREAK_:
            break_(dsp, instr);
            break;
        case DSP_CALL:
            call(dsp, instr);
            break;
        case DSP_CALLA_AX:
            calla_ax(dsp, instr);
            break;
        case DSP_CALLR:
            callr(dsp, instr);
            break;
        case DSP_CNTX_S:
            cntx_s(dsp, instr);
            break;
        case DSP_CNTX_R:
            cntx_r(dsp, instr);
            break;
        case DSP_RET:
            ret(dsp, instr);
            break;
        case DSP_RETI:
            reti(dsp, instr);
            break;
        case DSP_RETIC:
            retic(dsp, instr);
            break;
        case DSP_RETS:
            rets(dsp, instr);
            break;
        case DSP_PUSH_IMM16:
            push_imm16(dsp, instr);
            break;
        case DSP_PUSH_REG:
            push_reg(dsp, instr);
            break;
        case DSP_PUSH_ABE:
            push_abe(dsp, instr);
            break;
        case DSP_PUSH_ARARPSTTMOD:
            push_ararpsttmod(dsp, instr);
            break;
        case DSP_PUSH_PX:
            push_px(dsp, instr);
            break;
        case DSP_PUSH_R6:
            push_r6(dsp, instr);
            break;
        case DSP_PUSH_REPC:
            push_repc(dsp, instr);
            break;
        case DSP_PUSH_X0:
            push_x0(dsp, instr);
            break;
        case DSP_PUSH_X1:
            push_x1(dsp, instr);
            break;
        case DSP_PUSH_Y1:
            push_y1(dsp, instr);
            break;
        case DSP_PUSHA_AX:
            pusha_ax(dsp, instr);
            break;
        case DSP_PUSHA_BX:
            pusha_bx(dsp, instr);
            break;
        case DSP_POP_REG:
            pop_reg(dsp, instr);
            break;
        case DSP_POP_ABE:
            pop_abe(dsp, instr);
            break;
        case DSP_POP_ARARPSTTMOD:
            pop_ararpsttmod(dsp, instr);
            break;
        case DSP_POP_PX:
            pop_px(dsp, instr);
            break;
        case DSP_POP_R6:
            pop_r6(dsp, instr);
            break;
        case DSP_POP_REPC:
            pop_repc(dsp, instr);
            break;
        case DSP_POP_X0:
            pop_x0(dsp, instr);
            break;
        case DSP_POP_X1:
            pop_x1(dsp, instr);
            break;
        case DSP_POP_Y1:
            pop_y1(dsp, instr);
            break;
        case DSP_POPA:
            popa(dsp, instr);
            break;
        case DSP_REP_IMM:
            rep_imm(dsp, instr);
            break;
        case DSP_REP_REG:
            rep_reg(dsp, instr);
            break;
        case DSP_SHFC:
            shfc(dsp, instr);
            break;
        case DSP_SHFI:
            shfi(dsp, instr);
            break;
        case DSP_TSTB_MEMIMM8:
            tstb_memimm8(dsp, instr);
            break;
        case DSP_TSTB_RN_STEP:
            tstb_rn_step(dsp, instr);
            break;
        case DSP_TSTB_REG:
            tstb_reg(dsp, instr);
            break;
        case DSP_AND_:
            and_(dsp, instr);
            break;
        case DSP_DINT:
            dint(dsp, instr);
            break;
        case DSP_EINT:
            eint(dsp, instr);
            break;
        case DSP_EXP_REG:
            exp_reg(dsp, instr);
            break;
        case DSP_MODR:
            modr(dsp, instr);
            break;
        case DSP_MODR_I2:
            modr_i2(dsp, instr);
            break;
        case DSP_MODR_D2:
            modr_d2(dsp, instr);
            break;
        case DSP_MODR_EEMOD:
            modr_eemod(dsp, instr);
            break;
        case DSP_MOV_AB_AB:
            mov_ab_ab(dsp, instr);
            break;
        case DSP_MOV_Y1:
            mov_y1(dsp, instr);
            break;
        case DSP_MOV_ABLH_MEMIMM8:
            mov_ablh_memimm8(dsp, instr);
            break;
        case DSP_MOV_AXL_MEMIMM16:
            mov_axl_memimm16(dsp, instr);
            break;
        case DSP_MOV_AXL_MEMR7IMM7S:
            mov_axl_memr7imm7s(dsp, instr);
            break;
        case DSP_MOV_MEMIMM8_AB:
            mov_memimm8_ab(dsp, instr);
            break;
        case DSP_MOV_MEMIMM8_ABLH:
            mov_memimm8_ablh(dsp, instr);
            break;
        case DSP_MOV_MEMIMM8_RNOLD:
            mov_memimm8_rnold(dsp, instr);
            break;
        case DSP_MOV_MEMIMM16_AX:
            mov_memimm16_ax(dsp, instr);
            break;
        case DSP_MOV_IMM16_BX:
            mov_imm16_bx(dsp, instr);
            break;
        case DSP_MOV_IMM16_REG:
            mov_imm16_reg(dsp, instr);
            break;
        case DSP_MOV_IMM8S_RNOLD:
            mov_imm8s_rnold(dsp, instr);
            break;
        case DSP_MOV_SV_MEMIMM8:
            mov_sv_memimm8(dsp, instr);
            break;
        case DSP_MOV_SV_IMM8S:
            mov_sv_imm8s(dsp, instr);
            break;
        case DSP_MOV_IMM8_AXL:
            mov_imm8_axl(dsp, instr);
            break;
        case DSP_MOV_RN_STEP_BX:
            mov_rn_step_bx(dsp, instr);
            break;
        case DSP_MOV_RN_STEP_REG:
            mov_rn_step_reg(dsp, instr);
            break;
        case DSP_MOV_MEMR7IMM16_AX:
            mov_memr7imm16_ax(dsp, instr);
            break;
        case DSP_MOV_MEMR7IMM7S_AX:
            mov_memr7imm7s_ax(dsp, instr);
            break;
        case DSP_MOV_REG_BX:
            mov_reg_bx(dsp, instr);
            break;
        case DSP_MOV_MIXP_REG:
            mov_mixp_reg(dsp, instr);
            break;
        case DSP_MOV_RNOLD_MEMIMM8:
            mov_rnold_memimm8(dsp, instr);
            break;
        case DSP_MOV_REG_MIXP:
            mov_reg_mixp(dsp, instr);
            break;
        case DSP_MOV_REG_REG:
            mov_reg_reg(dsp, instr);
            break;
        case DSP_MOV_REG_RN_STEP:
            mov_reg_rn_step(dsp, instr);
            break;
        case DSP_MOV_SV_TO:
            mov_sv_to(dsp, instr);
            break;
        case DSP_LOAD_PAGE:
            load_page(dsp, instr);
            break;
        case DSP_LOAD_PS01:
            load_ps01(dsp, instr);
            break;
        case DSP_MOV_STTMOD:
            mov_sttmod(dsp, instr);
            break;
        case DSP_MOVP_REG:
            movp_reg(dsp, instr);
            break;
        case DSP_MOV_ABL_STTMOD:
            mov_abl_sttmod(dsp, instr);
            break;
        case DSP_MOV_STTMOD_ABL:
            mov_sttmod_abl(dsp, instr);
            break;
        case DSP_MOV_ARARP:
            mov_ararp(dsp, instr);
            break;
        case DSP_MOV_STEPI:
            mov_stepi(dsp, instr);
            break;
        case DSP_MOV_STEPJ:
            mov_stepj(dsp, instr);
            break;
        case DSP_MOV_R6:
            mov_r6(dsp, instr);
            break;
        case DSP_MOV_STEPI0:
            mov_stepi0(dsp, instr);
            break;
        case DSP_MOV_STEPJ0:
            mov_stepj0(dsp, instr);
            break;
        case DSP_MOV_A0H_STEPI0:
            mov_a0h_stepi0(dsp, instr);
            break;
        case DSP_MOV_A0H_STEPJ0:
            mov_a0h_stepj0(dsp, instr);
            break;
        case DSP_MOV_STEPI0_A0H:
            mov_stepi0_a0h(dsp, instr);
            break;
        case DSP_MOV_STEPJ0_A0H:
            mov_stepj0_a0h(dsp, instr);
            break;
        case DSP_MOV_ABL_ARARP:
            mov_abl_ararp(dsp, instr);
            break;
        case DSP_MOV_ARARP_ABL:
            mov_ararp_abl(dsp, instr);
            break;
        case DSP_MOV_AX_PC:
            mov_ax_pc(dsp, instr);
            break;
        case DSP_MOV_AB_P0:
            mov_ab_p0(dsp, instr);
            break;
        case DSP_MOV_P1_AB:
            mov_p1_ab(dsp, instr);
            break;
        case DSP_MOV2_PX_ARSTEP:
            mov2_px_arstep(dsp, instr);
            break;
        case DSP_MOV2_ARSTEP_PX:
            mov2_arstep_px(dsp, instr);
            break;
        case DSP_MOVA_AB_ARSTEP:
            mova_ab_arstep(dsp, instr);
            break;
        case DSP_MOVA_ARSTEP_AB:
            mova_arstep_ab(dsp, instr);
            break;
        case DSP_MOV_R6_REG:
            mov_r6_reg(dsp, instr);
            break;
        case DSP_MOV_REG_R6:
            mov_reg_r6(dsp, instr);
            break;
        case DSP_MOVS_MEMIMM8_AB:
            movs_memimm8_ab(dsp, instr);
            break;
        case DSP_MOVS_RN_STEP_AB:
            movs_rn_step_ab(dsp, instr);
            break;
        case DSP_MOVS_REG_AB:
            movs_reg_ab(dsp, instr);
            break;
        case DSP_MOVSI:
            movsi(dsp, instr);
            break;
        case DSP_MOV2_ABH_M:
            mov2_abh_m(dsp, instr);
            break;
        case DSP_EXCHANGE_JAI:
            exchange_jai(dsp, instr);
            break;
        case DSP_LIM:
            lim(dsp, instr);
            break;
        case DSP_CLRP0:
            clrp0(dsp, instr);
            break;
        case DSP_CLRP1:
            clrp1(dsp, instr);
            break;
        case DSP_CLRP:
            clrp(dsp, instr);
            break;
        case DSP_MAX_GT:
            max_gt(dsp, instr);
            break;
        case DSP_MIN_LT:
            min_lt(dsp, instr);
            break;
        case DSP_MMA_EMOD_EMOD_SYXX_XYXX_AC:
            mma_emod_emod_syxx_xyxx_ac(dsp, instr);
            break;
        case DSP_MMA_EMOD_EMOD_SYSX_SYSX_ZR:
            mma_emod_emod_sysx_sysx_zr(dsp, instr);
            break;
        case DSP_MMA_EMOD_EMOD_SYSX_SYSX_AC_2:
            mma_emod_emod_sysx_sysx_ac_2(dsp, instr);
            break;
        case DSP_MMA_MY_MY:
            mma_my_my(dsp, instr);
            break;
        default:
            EmuException::die("[DSP_Interpreter] Unrecognized instr $%04X", instr);
    }
}

bool is_alb_modifying(uint8_t op)
{
    switch (op)
    {
        case 0x0: //set
        case 0x1: //rst
        case 0x2: //chng
        case 0x3: //addv
        case 0x7: //subv
            return true;
        case 0x4: //tst0
        case 0x5: //tst1
        case 0x6: //cmpv
            return false;
        default:
            EmuException::die("[DSP_Interpreter] Unrecognized op $%02X in is_alb_modifying");
            return false;
    }
}

uint16_t do_alb_op(DSP &dsp, uint16_t a, uint16_t b, uint8_t op)
{
    uint16_t result = 0;
    switch (op)
    {
        case 0x0:
            //SET
            result = a | b;
            dsp.set_fm(result >> 15);
            break;
        case 0x1:
            //RST
            result = ~a & b;
            dsp.set_fm(result >> 15);
            break;
        case 0x2:
            //CHNG
            result = a ^ b;
            dsp.set_fm(result >> 15);
            break;
        case 0x3:
            //ADDV
        {
            uint32_t temp = a + b;
            dsp.set_fc((temp >> 16) != 0);
            dsp.set_fm((SignExtend<16, uint32_t>(b) + SignExtend<16, uint32_t>(a)) >> 31);
            result = temp & 0xFFFF;
        }
            break;
        case 0x4:
            //TST0
            result = (a & b) != 0;
            break;
        case 0x5:
            //TST1
            result = (a & ~b) != 0;
            break;
        case 0x6:
        case 0x7:
        {
            //CMPV/SUBV
            uint32_t temp = b - a;
            dsp.set_fc((temp >> 16) != 0);
            dsp.set_fm((SignExtend<16, uint32_t>(b) - SignExtend<16, uint32_t>(a)) >> 31);
            result = temp & 0xFFFF;
        }
            break;
        default:
            EmuException::die("[DSP_Interpreter] Unrecognized op $%02X in do_alb_op", op);
    }
    dsp.set_fz(result == 0);
    return result;
}

uint64_t signextend_alm(uint16_t value, uint8_t op)
{
    switch (op)
    {
        case 0x3:
        case 0x6:
        case 0x7:
            return SignExtend<16, uint64_t>(value);
        case 0x9:
        case 0xB:
            return SignExtend<32, uint64_t>(value << 16);
        default:
            return value;
    }
}

void do_alm_op(DSP &dsp, DSP_REG acc, uint64_t value, uint8_t op)
{
    uint64_t acc_value = dsp.get_acc(acc);
    uint64_t result;
    switch (op)
    {
        case 0x0:
            //OR
            acc_value |= value;
            dsp.set_acc_and_flag(acc, SignExtend<40>(acc_value));
            break;
        case 0x1:
            //AND
            acc_value &= value;
            dsp.set_acc_and_flag(acc, SignExtend<40>(acc_value));
            break;
        case 0x2:
            //XOR
            acc_value ^= value;
            dsp.set_acc_and_flag(acc, SignExtend<40>(acc_value));
            break;
        case 0x3:
            //ADD
            result = dsp.get_add_sub_result(acc_value, value, false);
            dsp.saturate_acc_with_flag(acc, result);
            break;
        case 0x4:
            //TST0
            acc_value &= 0xFFFF;
            dsp.set_fz((acc_value & value) == 0);
            break;
        case 0x5:
            //TST1
            acc_value &= 0xFFFF;
            dsp.set_fz((~acc_value & value) == 0);
            break;
        case 0x6:
            //CMP
            result = dsp.get_add_sub_result(acc_value, value, true);
            dsp.set_acc_flags(result);
            break;
        case 0x7:
            //SUB
            result = dsp.get_add_sub_result(acc_value, value, true);
            dsp.saturate_acc_with_flag(acc, result);
            break;
        case 0x9:
            //ADDH
            result = dsp.get_add_sub_result(acc_value, value, false);
            dsp.saturate_acc_with_flag(acc, result);
            break;
        case 0xA:
            //ADDL
            result = dsp.get_add_sub_result(acc_value, value, false);
            dsp.saturate_acc_with_flag(acc, result);
            break;
        case 0xB:
            //SUBH
            result = dsp.get_add_sub_result(acc_value, value, true);
            dsp.saturate_acc_with_flag(acc, result);
            break;
        case 0xC:
            //SUBL
            result = dsp.get_add_sub_result(acc_value, value, true);
            dsp.saturate_acc_with_flag(acc, result);
            break;
        case 0xD:
            //SQR
            value &= 0xFFFF;
            dsp.set_x(0, value);
            dsp.set_y(0, value);
            dsp.multiply(0, true, true);
            break;
        case 0xF:
            //CMPU
            result = dsp.get_add_sub_result(acc_value, value, true);
            dsp.set_acc_flags(result);
            break;
        default:
            EmuException::die("[DSP_Interpreter] Unrecognized alm op $%02X", op);
    }
}

void do_mul3_op(DSP &dsp, DSP_REG acc, uint8_t op)
{
    //Accumulate operation
    if (op >= 0x2)
    {
        uint64_t value = dsp.get_acc(acc);
        uint64_t product = dsp.get_product(0);

        //Special case for MAA/MAASU
        if (op == 0x4 || op == 0x7)
        {
            product >>= 16;
            product = SignExtend<24>(product);
        }

        uint64_t result = dsp.get_add_sub_result(value, product, false);
        dsp.saturate_acc_with_flag(acc, result);
    }

    switch (op)
    {
        case 0x0:
        case 0x2:
        case 0x4:
            //MPY/MAC/MAA
            dsp.multiply(0, true, true);
            break;
        case 0x1:
        case 0x6:
        case 0x7:
            //MPYSU/MACSU/MAASU
            dsp.multiply(0, false, true);
            break;
        case 0x3:
            //MACUS
            dsp.multiply(0, true, false);
            break;
        case 0x5:
            //MACUU
            dsp.multiply(0, false, false);
            break;
        default:
            EmuException::die("[DSP_Intepreter] Unrecognized mul3 op $%02X", op);
    }
}

void swap(DSP &dsp, uint16_t instr)
{
    DSP_REG s0, d0, s1, d1;
    int type = instr & 0xF;
    switch (type)
    {
        case 0x0:
            //A0B0
            s0 = d1 = DSP_REG_A0;
            s1 = d0 = DSP_REG_B0;
            break;
        default:
            s0 = d0 = s1 = d1 = DSP_REG_UNK;
            EmuException::die("[DSP_Interpreter] Unrecognized swap op $%02X", type);
    }

    uint64_t u = dsp.get_acc(s0);
    uint64_t v = dsp.get_acc(s1);
    dsp.saturate_acc_with_flag(d0, u);
    dsp.saturate_acc_with_flag(d1, v);
}

void alm_memimm8(DSP &dsp, uint16_t instr)
{
    uint8_t imm = instr & 0xFF;
    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = (instr >> 9) & 0xF;

    DSP_REG acc = get_ax_reg(ax);

    uint16_t value = dsp.read_from_page(imm);

    do_alm_op(dsp, acc, signextend_alm(value, op), op);
}

void alm_rn_step(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;
    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = (instr >> 9) & 0xF;

    DSP_REG acc = get_ax_reg(ax);

    uint16_t addr = dsp.rn_addr_and_modify(rn, step, false);
    uint16_t value = dsp.read_data_word(addr);

    do_alm_op(dsp, acc, signextend_alm(value, op), op);
}

void alm_reg(DSP &dsp, uint16_t instr)
{
    uint8_t reg = instr & 0x1F;
    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = (instr >> 9) & 0xF;

    DSP_REG a = get_register(reg);
    DSP_REG acc = get_ax_reg(ax);

    uint64_t value = 0;

    switch (a)
    {
        case DSP_REG_P:
            value = dsp.get_product(0);
            break;
        case DSP_REG_A0:
        case DSP_REG_A1:
            value = dsp.get_acc(a);
            break;
        default:
            value = dsp.get_reg16(a, false);
            value = signextend_alm(value, op);
            break;
    }

    do_alm_op(dsp, acc, value, op);
}

void alu_memimm16(DSP &dsp, uint16_t instr)
{
    uint16_t addr = dsp.fetch_code_word();
    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = instr & 0x7;

    DSP_REG acc = get_ax_reg(ax);

    do_alm_op(dsp, acc, signextend_alm(dsp.read_data_word(addr), op), op);
}

void alu_memr7imm16(DSP &dsp, uint16_t instr)
{
    uint16_t imm = dsp.read_data_r16();

    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = instr & 0x7;

    DSP_REG acc = get_ax_reg(ax);

    do_alm_op(dsp, acc, signextend_alm(imm, op), op);
}

void alu_imm16(DSP &dsp, uint16_t instr)
{
    uint16_t imm = dsp.fetch_code_word();
    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = (instr >> 9) & 0x7;

    DSP_REG acc = get_ax_reg(ax);

    do_alm_op(dsp, acc, signextend_alm(imm, op), op);
}

void alu_imm8(DSP &dsp, uint16_t instr)
{
    uint8_t imm = instr & 0xFF;
    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = (instr >> 9) & 0x7;

    DSP_REG acc = get_ax_reg(ax);

    uint64_t backup = 0;

    //Special case for AND
    if (op == 1)
        backup = dsp.get_acc(acc) & 0xFF00;

    do_alm_op(dsp, acc, signextend_alm(imm, op), op);

    if (op == 1)
    {
        uint64_t and_new = dsp.get_acc(acc) & 0xFFFFFFFFFFFF00FFULL;
        dsp.set_acc(acc, backup | and_new);
    }
}

void alu_memr7imm7s(DSP &dsp, uint16_t instr)
{
    int16_t imm = instr & 0x7F;
    imm = SignExtend<7>(imm);

    uint8_t ax = (instr >> 8) & 0x1;
    uint8_t op = (instr >> 9) & 0x7;

    DSP_REG acc = get_ax_reg(ax);

    uint16_t value = dsp.read_data_r7s(imm);
    do_alm_op(dsp, acc, signextend_alm(value, op), op);
}

void or_1(DSP &dsp, uint16_t instr)
{
    DSP_REG a = get_ab_reg((instr >> 10) & 0x3);
    DSP_REG b = get_ax_reg((instr >> 6) & 0x1);
    DSP_REG c = get_ax_reg((instr >> 5) & 0x1);

    uint64_t value = dsp.get_acc(a) | dsp.get_acc(b);
    dsp.set_acc_and_flag(c, value);
}

void or_2(DSP &dsp, uint16_t instr)
{
    DSP_REG a = get_ax_reg((instr >> 8) & 0x1);
    DSP_REG b = get_bx_reg((instr >> 1) & 0x1);
    DSP_REG c = get_ax_reg(instr & 0x1);

    uint64_t value = dsp.get_acc(a) | dsp.get_acc(b);
    dsp.set_acc_and_flag(c, value);
}

void alb_memimm8(DSP &dsp, uint16_t instr)
{
    uint16_t imm = dsp.fetch_code_word();

    uint8_t addr = instr & 0xFF;
    uint8_t op = (instr >> 9) & 0x7;

    uint16_t result = do_alb_op(dsp, imm, dsp.read_from_page(addr), op);

    if (is_alb_modifying(op))
        dsp.write_to_page(addr, result);
}

void alb_rn_step(DSP &dsp, uint16_t instr)
{
    uint16_t imm = dsp.fetch_code_word();

    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;
    uint8_t op = (instr >> 9) & 0x7;

    uint16_t addr = dsp.rn_addr_and_modify(rn, step, false);

    uint16_t result = do_alb_op(dsp, imm, dsp.read_data_word(addr), op);

    if (is_alb_modifying(op))
        dsp.write_data_word(addr, result);
}

void alb_reg(DSP &dsp, uint16_t instr)
{
    uint16_t imm = dsp.fetch_code_word();

    DSP_REG reg = get_register(instr & 0x1F);
    uint8_t op = (instr >> 9) & 0x7;

    uint16_t reg_value = 0;

    if (reg == DSP_REG_P)
        EmuException::die("[DSP_Interpreter] DSP reg P used for alb_reg");
    else
        reg_value = dsp.get_reg16(reg, false);

    uint16_t result = do_alb_op(dsp, imm, reg_value, op);

    if (is_alb_modifying(op))
    {
        switch (reg)
        {
            case DSP_REG_A0l:
            case DSP_REG_A1l:
            case DSP_REG_B0l:
            case DSP_REG_B1l:
                dsp.set_acc_lo(reg, result);
                break;
            case DSP_REG_A0h:
            case DSP_REG_A1h:
            case DSP_REG_B0h:
            case DSP_REG_B1h:
                dsp.set_acc_hi(reg, result);
                break;
            default:
                dsp.set_reg16(reg, result);
                break;
        }
    }
}

void add_ab_bx(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 10) & 0x3);
    DSP_REG bx = get_bx_reg(instr & 0x1);

    uint64_t a = dsp.get_acc(ab);
    uint64_t b = dsp.get_acc(bx);

    uint64_t result = dsp.get_add_sub_result(a, b, false);
    dsp.saturate_acc_with_flag(bx, result);
}

void add_bx_ax(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg(instr & 0x1);
    DSP_REG bx = get_bx_reg((instr >> 1) & 0x1);

    uint64_t a = dsp.get_acc(bx);
    uint64_t b = dsp.get_acc(ax);

    uint64_t result = dsp.get_add_sub_result(a, b, false);
    dsp.saturate_acc_with_flag(ax, result);
}

void add_p1_ax(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg(instr & 0x1);

    uint64_t b = dsp.get_product(1);
    uint64_t a = dsp.get_acc(ax);

    uint64_t result = dsp.get_add_sub_result(a, b, false);
    dsp.saturate_acc_with_flag(ax, result);
}

void add_px_bx(DSP &dsp, uint16_t instr)
{
    DSP_REG bx = get_bx_reg(instr & 0x1);
    uint64_t a = dsp.get_product((instr >> 1) & 0x1);
    uint64_t b = dsp.get_acc(bx);

    uint64_t result = dsp.get_add_sub_result(a, b, false);
    dsp.saturate_acc_with_flag(bx, result);
}

void sub_ab_bx(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 3) & 0x3);
    DSP_REG bx = get_bx_reg((instr >> 8) & 0x1);

    uint64_t a = dsp.get_acc(ab);
    uint64_t b = dsp.get_acc(bx);

    uint64_t result = dsp.get_add_sub_result(b, a, true);
    dsp.saturate_acc_with_flag(bx, result);
}

void sub_bx_ax(DSP &dsp, uint16_t instr)
{
    DSP_REG bx = get_bx_reg((instr >> 4) & 0x1);
    DSP_REG ax = get_ax_reg((instr >> 3) & 0x1);

    uint64_t a = dsp.get_acc(bx);
    uint64_t b = dsp.get_acc(ax);

    uint64_t result = dsp.get_add_sub_result(b, a, true);
    dsp.saturate_acc_with_flag(ax, result);
}

void set_sttmod(DSP &dsp, uint16_t instr)
{
    DSP_REG sttmod = get_sttmod_reg(instr & 0x7);

    uint16_t imm = dsp.fetch_code_word();

    uint16_t result = do_alb_op(dsp, imm, dsp.get_reg16(sttmod, false), 0);

    dsp.set_reg16(sttmod, result);
}

void rst_sttmod(DSP &dsp, uint16_t instr)
{
    DSP_REG sttmod = get_sttmod_reg(instr & 0x7);

    uint16_t imm = dsp.fetch_code_word();

    uint16_t result = do_alb_op(dsp, imm, dsp.get_reg16(sttmod, false), 1);

    dsp.set_reg16(sttmod, result);
}

void app_zr(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 2) & 0x3);
    bool sub = (instr >> 1) & 0x1;
    bool p1_align = instr & 0x1;
    dsp.product_sum(0, ab, false, false, sub, p1_align);
}

void app_ac_add_pp_pp(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 2) & 0x3);

    dsp.product_sum(1, ab, false, false, false, false);
}

void app_ac_add_pp_pa(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 2) & 0x3);

    dsp.product_sum(1, ab, false, false, false, true);
}

void app_ac_add_pa_pa(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 2) & 0x3);

    dsp.product_sum(1, ab, false, true, false, true);
}

void mul_arstep_imm16(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t arstep = (instr >> 3) & 0x3;
    DSP_REG ax = get_ax_reg((instr >> 11) & 0x1);
    uint8_t op = (instr >> 8) & 0x7;

    uint16_t addr = dsp.rn_addr_and_modify(rn, arstep, false);

    dsp.set_y(0, dsp.read_data_word(addr));
    dsp.set_x(0, dsp.fetch_code_word());

    do_mul3_op(dsp, ax, op);
}

void mul_y0_arstep(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t arstep = (instr >> 3) & 0x3;
    DSP_REG ax = get_ax_reg((instr >> 11) & 0x1);
    uint8_t op = (instr >> 8) & 0x7;

    uint16_t addr = dsp.rn_addr_and_modify(rn, arstep, false);

    dsp.set_x(0, dsp.read_data_word(addr));

    do_mul3_op(dsp, ax, op);
}

void mul_y0_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);
    DSP_REG ax = get_ax_reg((instr >> 11) & 0x1);
    uint8_t op = (instr >> 8) & 0x7;

    dsp.set_x(0, dsp.get_reg16(reg, false));

    do_mul3_op(dsp, ax, op);
}

void mul_r45_arstep(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 11) & 0x1);
    uint8_t op = (instr >> 8) & 0x7;

    uint16_t addr_y = dsp.rn_addr_and_modify(((instr >> 2) & 0x1) + 4, ((instr >> 5) & 0x3), false);
    uint16_t addr_x = dsp.rn_addr_and_modify(instr & 0x3, (instr >> 3) & 0x3, false);

    dsp.set_y(0, dsp.read_data_word(addr_y));
    dsp.set_x(0, dsp.read_data_word(addr_x));

    do_mul3_op(dsp, ax, op);
}

void mul_y0_memimm8(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 11) & 0x1);
    uint8_t op = (instr >> 9) & 0x3;
    uint8_t memimm = instr & 0xFF;

    uint16_t value = dsp.read_from_page(memimm);

    dsp.set_x(0, value);

    do_mul3_op(dsp, ax, op << 1);
}

void mpyi(DSP &dsp, uint16_t instr)
{
    uint16_t imm = instr & 0xFF;
    imm = SignExtend<16>(imm);

    dsp.set_x(0, imm);
    dsp.multiply(0, true, true);
}

void moda4(DSP &dsp, uint16_t instr)
{
    uint8_t ax = (instr >> 12) & 0x1;
    uint8_t op = (instr >> 4) & 0xF;
    uint8_t cond = instr & 0xF;

    DSP_REG acc = get_ax_reg(ax);

    if (dsp.meets_condition(cond))
    {
        switch (op)
        {
            case 0x0:
                //SHR
                dsp.shift_reg_40(dsp.get_acc(acc), acc, 0xFFFF);
                break;
            case 0x2:
                //SHL
                dsp.shift_reg_40(dsp.get_acc(acc), acc, 1);
                break;
            case 0x6:
                //CLR
                dsp.saturate_acc_with_flag(acc, 0);
                break;
            case 0x8:
                //NOT
            {
                uint64_t value = ~dsp.get_acc(acc);
                dsp.set_acc_and_flag(acc, value);
            }
                break;
            case 0x9:
                //NEG
            {
                uint64_t value = dsp.get_acc(acc);
                dsp.set_fc(value != 0);
                if (value == 0xFFFFFF8000000000ULL)
                {
                    dsp.set_fv(true);
                    dsp.set_fvl(true);
                }
                uint64_t result = SignExtend<40, uint64_t>(~value + 1);
                dsp.saturate_acc_with_flag(acc, result);
            }
                break;
            case 0xA:
                //RND
            {
                uint64_t value = dsp.get_acc(acc);
                uint64_t result = dsp.get_add_sub_result(value, 0x8000, false);
                dsp.saturate_acc_with_flag(acc, result);
            }
                break;
            case 0xC:
                //CLRR
                dsp.saturate_acc_with_flag(acc, 0x8000);
                break;
            case 0xD:
                //INC
            {
                uint64_t value = dsp.get_acc(acc);
                uint64_t result = dsp.get_add_sub_result(value, 1, false);
                dsp.saturate_acc_with_flag(acc, result);
            }
                break;
            case 0xE:
                //DEC
            {
                uint64_t value = dsp.get_acc(acc);
                uint64_t result = dsp.get_add_sub_result(value, 1, true);
                dsp.saturate_acc_with_flag(acc, result);
            }
                break;
            case 0xF:
                //COPY
            {
                uint64_t value = dsp.get_acc((acc == DSP_REG_A0) ? DSP_REG_A1 : DSP_REG_A0);
                dsp.saturate_acc_with_flag(acc, value);
            }
                break;
            default:
                EmuException::die("[DSP_Interpreter] Unrecognized MODA4 op $%02X", op);
        }
    }
}

void moda3(DSP &dsp, uint16_t instr)
{
    uint8_t bx = (instr >> 12) & 0x1;
    uint8_t op = (instr >> 4) & 0x7;
    uint8_t cond = instr & 0xF;

    DSP_REG acc = get_bx_reg(bx);

    if (dsp.meets_condition(cond))
    {
        switch (op)
        {
            case 0x2:
                //SHL
                dsp.shift_reg_40(dsp.get_acc(acc), acc, 1);
                break;
            case 0x6:
                //CLR
                dsp.saturate_acc_with_flag(acc, 0);
                break;
            default:
                EmuException::die("[DSP_Interpreter] Unrecognized MODA3 op $%02X", op);
        }
    }
}

void bkrep_imm8(DSP &dsp, uint16_t instr)
{
    uint8_t imm = instr & 0xFF;

    uint32_t addr = dsp.fetch_code_word();
    addr |= dsp.get_pc() & 0x30000;

    dsp.block_repeat(imm, addr);
}

void bkrep_reg(DSP &dsp, uint16_t instr)
{
    uint32_t addr = dsp.fetch_code_word();
    uint32_t addr_hi = (instr >> 5) & 0x3;
    addr |= addr_hi << 16;
    DSP_REG reg = get_register(instr & 0x1F);

    uint16_t lc = dsp.get_reg16(reg, false);

    dsp.block_repeat(lc, addr);
}

void bkrep_r6(DSP &dsp, uint16_t instr)
{
    uint32_t addr = dsp.fetch_code_word();
    uint32_t addr_hi = instr & 0x3;
    addr |= addr_hi << 16;

    uint16_t lc = dsp.get_reg16(DSP_REG_R6, false);

    dsp.block_repeat(lc, addr);
}

void bkreprst_memsp(DSP &dsp, uint16_t instr)
{
    uint16_t sp = dsp.get_reg16(DSP_REG_SP, false);
    uint16_t value = dsp.restore_block_repeat(sp);
    dsp.set_reg16(DSP_REG_SP, value);
}

void bkrepsto_memsp(DSP &dsp, uint16_t instr)
{
    uint16_t sp = dsp.get_reg16(DSP_REG_SP, false);
    uint16_t value = dsp.store_block_repeat(sp);
    dsp.set_reg16(DSP_REG_SP, value);
}

void banke(DSP &dsp, uint16_t instr)
{
    uint8_t flags = instr & 0x3F;

    dsp.banke(flags);
}

void br(DSP &dsp, uint16_t instr)
{
    uint32_t addr = dsp.fetch_code_word();
    uint32_t addr_hi = (instr >> 4) & 0x3;
    uint8_t cond = instr & 0xF;
    addr |= addr_hi << 16;

    if (dsp.meets_condition(cond))
        dsp.set_pc(addr);
}

void brr(DSP &dsp, uint16_t instr)
{
    int16_t offset = (instr >> 4) & 0x7F;
    uint8_t cond = instr & 0xF;

    offset = SignExtend<7>(offset);

    if (dsp.meets_condition(cond))
    {
        dsp.branch(offset);
        if (offset == -1)
            dsp.halt();
    }
}

void break_(DSP &dsp, uint16_t instr)
{
    dsp.break_();
}

void call(DSP &dsp, uint16_t instr)
{
    uint32_t addr = dsp.fetch_code_word();
    uint32_t addr_hi = (instr >> 4) & 0x3;
    uint8_t cond = instr & 0xF;
    addr |= addr_hi << 16;

    if (dsp.meets_condition(cond))
    {
        dsp.push_pc();
        dsp.set_pc(addr);
    }
}

void calla_ax(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 4) & 0x1);

    dsp.push_pc();
    dsp.set_pc(dsp.get_acc(ax) & 0x3FFFF);
}

void callr(DSP &dsp, uint16_t instr)
{
    int16_t offset = (instr >> 4) & 0x7F;
    offset = SignExtend<7>(offset);
    uint8_t cond = instr & 0xF;

    if (dsp.meets_condition(cond))
    {
        dsp.push_pc();
        uint32_t new_addr = dsp.get_pc() + offset;
        dsp.set_pc(new_addr);
    }
}

void cntx_s(DSP &dsp, uint16_t instr)
{
    dsp.save_context();
}

void cntx_r(DSP &dsp, uint16_t instr)
{
    dsp.restore_context();
}

void ret(DSP &dsp, uint16_t instr)
{
    uint8_t cond = instr & 0xF;

    if (dsp.meets_condition(cond))
        dsp.pop_pc();
}

void reti(DSP &dsp, uint16_t instr)
{
    uint8_t cond = instr & 0xF;

    if (dsp.meets_condition(cond))
    {
        dsp.pop_pc();
        dsp.set_master_int_enable(true);
    }
}

void retic(DSP &dsp, uint16_t instr)
{
    uint8_t cond = instr & 0xF;

    if (dsp.meets_condition(cond))
    {
        dsp.pop_pc();
        dsp.set_master_int_enable(true);
        dsp.restore_context();
    }
}

void rets(DSP &dsp, uint16_t instr)
{
    uint8_t imm = instr & 0xFF;

    dsp.pop_pc();

    uint16_t sp = dsp.get_reg16(DSP_REG_SP, false);
    sp += imm;
    dsp.set_reg16(DSP_REG_SP, sp);
}

void push_imm16(DSP &dsp, uint16_t instr)
{
    uint16_t value = dsp.fetch_code_word();

    dsp.push16(value);
}

void push_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    uint16_t value = dsp.get_reg16(reg, true);

    dsp.push16(value);
}

void push_abe(DSP &dsp, uint16_t instr)
{
    DSP_REG abe = get_abe_reg((instr >> 1) & 0x3);

    uint16_t value = (dsp.get_saturated_acc(abe) >> 32) & 0xFFFF;

    dsp.push16(value);
}

void push_ararpsttmod(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_ararpsttmod_reg(instr & 0xF);

    uint16_t value = dsp.get_reg16(reg, false);

    dsp.push16(value);
}

void push_px(DSP &dsp, uint16_t instr)
{
    uint32_t value = dsp.get_product((instr >> 1) & 0x1);

    dsp.push16(value & 0xFFFF);
    dsp.push16(value >> 16);
}

void push_r6(DSP &dsp, uint16_t instr)
{
    dsp.push16(dsp.get_rn(6));
}

void push_repc(DSP &dsp, uint16_t instr)
{
    dsp.push16(dsp.get_repc());
}

void push_x0(DSP &dsp, uint16_t instr)
{
    dsp.push16(dsp.get_x(0));
}

void push_x1(DSP &dsp, uint16_t instr)
{
    dsp.push16(dsp.get_x(1));
}

void push_y1(DSP &dsp, uint16_t instr)
{
    dsp.push16(dsp.get_y(1));
}

void pusha_ax(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 6) & 0x1);

    uint32_t value = dsp.get_saturated_acc(ax) & 0xFFFFFFFF;

    dsp.push16(value & 0xFFFF);
    dsp.push16(value >> 16);
}

void pusha_bx(DSP &dsp, uint16_t instr)
{
    DSP_REG bx = get_bx_reg((instr >> 6) & 0x1);

    uint32_t value = dsp.get_saturated_acc(bx) & 0xFFFFFFFF;

    dsp.push16(value & 0xFFFF);
    dsp.push16(value >> 16);
}

void pop_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    uint16_t value = dsp.pop16();

    dsp.set_reg16(reg, value);
}

void pop_abe(DSP &dsp, uint16_t instr)
{
    DSP_REG abe = get_abe_reg(instr & 0x3);

    uint64_t acc = dsp.get_acc(abe);

    uint32_t value = SignExtend<8, uint32_t>(dsp.pop16() & 0xFF);

    dsp.set_acc_and_flag(abe, (acc & 0xFFFFFFFF) | (uint64_t)value << 32ULL);
}

void pop_ararpsttmod(DSP &dsp, uint16_t instr)
{
    DSP_REG ararpsttmod = get_ararpsttmod_reg((instr >> 8) & 0xF);

    uint16_t value = dsp.pop16();

    dsp.set_reg16(ararpsttmod, value);
}

void pop_px(DSP &dsp, uint16_t instr)
{
    uint16_t hi = dsp.pop16();
    uint16_t lo = dsp.pop16();

    uint32_t value = lo | (hi << 16);

    dsp.set_product(instr & 0x1, value);
}

void pop_r6(DSP &dsp, uint16_t instr)
{
    uint16_t value = dsp.pop16();

    dsp.set_reg16(DSP_REG_R6, value);
}

void pop_repc(DSP &dsp, uint16_t instr)
{
    uint16_t value = dsp.pop16();

    dsp.set_repc(value);
}

void pop_x0(DSP &dsp, uint16_t instr)
{
    uint16_t value = dsp.pop16();

    dsp.set_x(0, value);
}

void pop_x1(DSP &dsp, uint16_t instr)
{
    uint16_t value = dsp.pop16();

    dsp.set_x(1, value);
}

void pop_y1(DSP &dsp, uint16_t instr)
{
    uint16_t value = dsp.pop16();

    dsp.set_y(1, value);
}

void popa(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg(instr & 0x3);

    uint16_t h = dsp.pop16();
    uint16_t l = dsp.pop16();

    uint64_t value = SignExtend<32, uint64_t>(l | (h << 16));

    dsp.set_acc_and_flag(ab, value);
}

void rep_imm(DSP &dsp, uint16_t instr)
{
    uint8_t value = instr & 0xFF;
    dsp.repeat(value);
}

void rep_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);
    dsp.repeat(dsp.get_reg16(reg, false));
}

void shfc(DSP &dsp, uint16_t instr)
{
    DSP_REG a = get_ab_reg((instr >> 10) & 0x3);
    DSP_REG b = get_ab_reg((instr >> 5) & 0x3);
    uint8_t cond = instr & 0xF;

    if (dsp.meets_condition(cond))
    {
        uint64_t value = dsp.get_acc(a);
        dsp.shift_reg_40(value, b, dsp.get_sv());
    }
}

void shfi(DSP &dsp, uint16_t instr)
{
    uint8_t ab_a = (instr >> 10) & 0x3;
    uint8_t ab_b = (instr >> 7) & 0x3;
    int16_t shift = instr & 0x3F;
    shift = SignExtend<6>(shift);

    uint64_t value = dsp.get_acc(get_ab_reg(ab_a));
    DSP_REG dest = get_ab_reg(ab_b);

    dsp.shift_reg_40(value, dest, shift);
}

void tstb_memimm8(DSP &dsp, uint16_t instr)
{
    uint8_t memimm = instr & 0xFF;
    uint8_t imm = (instr >> 8) & 0xF;

    uint16_t value = dsp.read_from_page(memimm);

    dsp.set_fz((value >> imm) & 0x1);
}

void tstb_rn_step(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;
    uint8_t imm = (instr >> 8) & 0xF;

    uint16_t addr = dsp.rn_addr_and_modify(rn, step, false);
    uint16_t value = dsp.read_data_word(addr);

    dsp.set_fz((value >> imm) & 0x1);
}

void tstb_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    uint8_t imm = (instr >> 8) & 0xF;
    uint16_t value = dsp.get_reg16(reg, true);

    dsp.set_fz((value >> imm) & 0x1);
}

void and_(DSP &dsp, uint16_t instr)
{
    DSP_REG a = get_ab_reg((instr >> 2) & 0x3);
    DSP_REG b = get_ab_reg(instr & 0x3);
    DSP_REG c = get_ax_reg((instr >> 12) & 0x1);

    uint64_t value = dsp.get_acc(a) & dsp.get_acc(b);
    dsp.set_acc_and_flag(c, value);
}

void dint(DSP &dsp, uint16_t instr)
{
    dsp.set_master_int_enable(false);
}

void eint(DSP &dsp, uint16_t instr)
{
    dsp.set_master_int_enable(true);
}

void exp_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    uint64_t value = 0;
    if (reg == DSP_REG_A0 || reg == DSP_REG_A1)
        value = dsp.get_acc(reg);
    else
        value = SignExtend<32>((uint64_t)dsp.get_reg16(reg, false) << 16);

    dsp.set_sv(dsp.exp(value));
}

void modr(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;

    dsp.rn_and_modify(rn, step, false);
    dsp.check_fr(rn);
}

void modr_i2(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;

    dsp.rn_and_modify(rn, 4, false);
    dsp.check_fr(rn);
}

void modr_d2(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;

    dsp.rn_and_modify(rn, 5, false);
    dsp.check_fr(rn);
}

void modr_eemod(DSP &dsp, uint16_t instr)
{
    //TODO: Checkme
    uint8_t ri = dsp.get_arprni((instr >> 10) & 0x3);
    uint8_t rj = dsp.get_arprnj((instr >> 10) & 0x3);

    uint8_t stepi = dsp.get_arpstepi(instr & 0x3);
    uint8_t stepj = dsp.get_arpstepj((instr >> 5) & 0x3);

    dsp.rn_and_modify(ri, stepi, false);
    dsp.rn_and_modify(rj, stepj, false);
}

void mov_ab_ab(DSP &dsp, uint16_t instr)
{
    DSP_REG a = get_ab_reg((instr >> 10) & 0x3);
    DSP_REG b = get_ab_reg((instr >> 5) & 0x3);

    uint64_t value = dsp.get_acc(a);
    dsp.saturate_acc_with_flag(b, value);
}

void mov_y1(DSP &dsp, uint16_t instr)
{
    uint8_t abl = instr & 0x3;

    uint16_t value = dsp.get_reg16(get_abl_reg(abl), true);
    dsp.set_y(1, value);
}

void mov_ablh_memimm8(DSP &dsp, uint16_t instr)
{
    uint8_t imm8 = instr & 0xFF;
    uint8_t ablh = (instr >> 9) & 0x7;

    uint16_t value = dsp.get_reg16(get_ablh_reg(ablh), true);

    dsp.write_to_page(imm8, value);
}

void mov_axl_memimm16(DSP &dsp, uint16_t instr)
{
    uint16_t addr = dsp.fetch_code_word();

    DSP_REG axl = get_axl_reg((instr >> 8) & 0x1);

    dsp.write_data_word(addr, dsp.get_reg16(axl, true));
}

void mov_axl_memr7imm7s(DSP &dsp, uint16_t instr)
{
    DSP_REG axl = get_axl_reg((instr >> 8) & 0x1);

    int16_t imm = SignExtend<7>((instr & 0x7F));

    dsp.write_data_r7s(imm, dsp.get_reg16(axl, true));
}

void mov_memimm8_ab(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 11) & 0x3);

    uint8_t imm = instr & 0xFF;

    dsp.set_reg16(ab, dsp.read_from_page(imm));
}

void mov_memimm8_ablh(DSP &dsp, uint16_t instr)
{
    DSP_REG ablh = get_ablh_reg((instr >> 10) & 0x7);

    uint8_t imm = instr & 0xFF;

    dsp.set_reg16(ablh, dsp.read_from_page(imm));
}

void mov_memimm8_rnold(DSP &dsp, uint16_t instr)
{
    DSP_REG rnold = get_rnold((instr >> 10) & 0x7);

    uint8_t imm = instr & 0xFF;

    dsp.set_reg16(rnold, dsp.read_from_page(imm));
}

void mov_memimm16_ax(DSP &dsp, uint16_t instr)
{
    uint16_t addr = dsp.fetch_code_word();

    DSP_REG ax = get_ax_reg((instr >> 8) & 0x1);

    dsp.set_reg16(ax, dsp.read_data_word(addr));
}

void mov_imm16_bx(DSP &dsp, uint16_t instr)
{
    uint16_t word = dsp.fetch_code_word();

    DSP_REG bx = get_bx_reg((instr >> 8) & 0x1);

    dsp.set_reg16(bx, word);
}

void mov_imm16_reg(DSP &dsp, uint16_t instr)
{
    uint16_t word = dsp.fetch_code_word();

    uint8_t reg = instr & 0x1F;

    dsp.set_reg16(get_register(reg), word);
}

void mov_imm8s_rnold(DSP &dsp, uint16_t instr)
{
    uint16_t imm = instr & 0xFF;
    imm = SignExtend<8>(imm);

    DSP_REG rnold = get_rnold((instr >> 10) & 0x7);

    dsp.set_reg16(rnold, imm);
}

void mov_sv_memimm8(DSP &dsp, uint16_t instr)
{
    uint8_t imm = instr & 0xFF;

    dsp.set_sv(dsp.read_from_page(imm));
}

void mov_sv_imm8s(DSP &dsp, uint16_t instr)
{
    uint16_t imm = instr & 0xFF;
    imm = SignExtend<8>(imm);

    dsp.set_sv(imm);
}

void mov_imm8_axl(DSP &dsp, uint16_t instr)
{
    uint8_t imm = instr & 0xFF;

    DSP_REG axl = get_axl_reg((instr >> 12) & 0x1);

    dsp.set_reg16(axl, imm);
}

void mov_rn_step_bx(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;

    uint16_t addr = dsp.rn_addr_and_modify(rn, step, false);

    DSP_REG bx = get_bx_reg((instr >> 8) & 0x1);

    dsp.set_reg16(bx, dsp.read_data_word(addr));
}

void mov_rn_step_reg(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;

    uint16_t addr = dsp.rn_addr_and_modify(rn, step, false);

    DSP_REG reg = get_register((instr >> 5) & 0x1F);

    dsp.set_reg16(reg, dsp.read_data_word(addr));
}

void mov_memr7imm16_ax(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 8) & 0x1);

    uint16_t value = dsp.read_data_r16();

    dsp.set_reg16(ax, value);
}

void mov_memr7imm7s_ax(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 8) & 0x1);

    int16_t imm = SignExtend<7>((instr & 0x7F));

    uint16_t value = dsp.read_data_r7s(imm);

    dsp.set_reg16(ax, value);
}

void mov_reg_bx(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);
    DSP_REG bx = get_bx_reg((instr >> 5) & 0x1);

    if (reg == DSP_REG_P)
    {
        uint64_t value = dsp.get_product(0);
        dsp.saturate_acc_with_flag(bx, value);
    }
    else if (reg == DSP_REG_A0 || reg == DSP_REG_A1)
    {
        uint64_t value = dsp.get_acc(reg);
        dsp.saturate_acc_with_flag(bx, value);
    }
    else
    {
        uint16_t value = dsp.get_reg16(reg, true);
        dsp.set_reg16(bx, value);
    }
}

void mov_mixp_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    dsp.set_reg16(reg, dsp.get_mixp());
}

void mov_rnold_memimm8(DSP &dsp, uint16_t instr)
{
    DSP_REG rnold = get_rnold((instr >> 9) & 0x7);

    uint8_t imm = instr & 0xFF;

    uint16_t value = dsp.get_reg16(rnold, false);
    dsp.write_to_page(imm, value);
}

void mov_reg_mixp(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    dsp.set_mixp(dsp.get_reg16(reg, true));
}

void mov_reg_reg(DSP &dsp, uint16_t instr)
{
    uint8_t reg_a = instr & 0x1F;
    uint8_t reg_b = (instr >> 5) & 0x1F;

    DSP_REG a = get_register(reg_a);
    DSP_REG b = get_register(reg_b);

    if (a == DSP_REG_P)
    {
        b = get_mov_from_p_reg(reg_b);
        uint64_t value = dsp.get_product(0);
        dsp.saturate_acc_with_flag(b, value);
    }
    else if (a == DSP_REG_PC)
    {
        if (b == DSP_REG_A0 || b == DSP_REG_A1)
            dsp.saturate_acc_with_flag(b, dsp.get_pc());
        else
            dsp.set_reg16(b, dsp.get_pc() & 0xFFFF);
    }
    else
    {
        uint16_t value = dsp.get_reg16(a, true);
        dsp.set_reg16(b, value);
    }
}

void mov_reg_rn_step(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;

    uint16_t addr = dsp.rn_addr_and_modify(rn, step, false);

    DSP_REG reg = get_register((instr >> 5) & 0x1F);
    uint16_t value = dsp.get_reg16(reg, true);

    dsp.write_data_word(addr, value);
}

void mov_sv_to(DSP &dsp, uint16_t instr)
{
    uint8_t imm = instr & 0xFF;
    dsp.write_to_page(imm, dsp.get_sv());
}

void load_page(DSP &dsp, uint16_t instr)
{
    dsp.set_page(instr & 0xFF);
}

void load_ps01(DSP &dsp, uint16_t instr)
{
    dsp.set_ps01(instr & 0xF);
}

void mov_sttmod(DSP &dsp, uint16_t instr)
{
    uint16_t word = dsp.fetch_code_word();

    uint8_t sttmod = instr & 0x7;

    dsp.set_reg16(get_sttmod_reg(sttmod), word);
}

void movp_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);
    DSP_REG ax = get_ax_reg((instr >> 5) & 0x1);

    uint32_t addr = dsp.get_acc(ax) & 0x3FFFF;

    uint16_t value = dsp.read_program_word(addr);

    dsp.set_reg16(reg, value);
}

void mov_abl_sttmod(DSP &dsp, uint16_t instr)
{
    DSP_REG sttmod = get_sttmod_reg(instr & 0x7);
    DSP_REG abl = get_abl_reg((instr >> 3) & 0x3);

    uint16_t value = dsp.get_reg16(abl, true);
    dsp.set_reg16(sttmod, value);
}

void mov_sttmod_abl(DSP &dsp, uint16_t instr)
{
    DSP_REG sttmod = get_sttmod_reg(instr & 0x7);
    DSP_REG abl = get_abl_reg((instr >> 10) & 0x3);

    uint16_t value = dsp.get_reg16(sttmod, false);
    dsp.set_reg16(abl, value);
}

void mov_ararp(DSP &dsp, uint16_t instr)
{
    uint16_t word = dsp.fetch_code_word();

    dsp.set_reg16(get_ararp_reg(instr & 0x7), word);
}

void mov_stepi(DSP &dsp, uint16_t instr)
{
    uint16_t imm = instr & 0x7F;

    dsp.set_stepi(imm);
}

void mov_stepj(DSP &dsp, uint16_t instr)
{
    uint16_t imm = instr & 0x7F;

    dsp.set_stepj(imm);
}

void mov_r6(DSP &dsp, uint16_t instr)
{
    uint16_t word = dsp.fetch_code_word();

    dsp.set_reg16(DSP_REG_R6, word);
}

void mov_stepi0(DSP &dsp, uint16_t instr)
{
    uint16_t word = dsp.fetch_code_word();

    dsp.set_stepi0(word);
}

void mov_stepj0(DSP &dsp, uint16_t instr)
{
    uint16_t word = dsp.fetch_code_word();

    dsp.set_stepj0(word);
}

void mov_a0h_stepi0(DSP &dsp, uint16_t instr)
{
    dsp.set_stepi0(dsp.get_reg16(DSP_REG_A0h, true));
}

void mov_a0h_stepj0(DSP &dsp, uint16_t instr)
{
    dsp.set_stepj0(dsp.get_reg16(DSP_REG_A0h, true));
}

void mov_stepi0_a0h(DSP &dsp, uint16_t instr)
{
    dsp.set_reg16(DSP_REG_A0h, dsp.get_stepi0());
}

void mov_stepj0_a0h(DSP &dsp, uint16_t instr)
{
    dsp.set_reg16(DSP_REG_A0h, dsp.get_stepj0());
}

void mov_abl_ararp(DSP &dsp, uint16_t instr)
{
    DSP_REG ararp = get_ararp_reg(instr & 0x7);
    DSP_REG abl = get_abl_reg((instr >> 3) & 0x3);

    dsp.set_reg16(ararp, dsp.get_reg16(abl, true));
}

void mov_ararp_abl(DSP &dsp, uint16_t instr)
{
    DSP_REG ararp = get_ararp_reg(instr & 0x7);
    DSP_REG abl = get_abl_reg((instr >> 3) & 0x3);

    dsp.set_reg16(abl, dsp.get_reg16(ararp, false));
}

void mov_ax_pc(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 8) & 0x1);

    uint64_t value = dsp.get_acc(ax);

    dsp.set_pc(value & 0xFFFFFFFF);
}

void mov_ab_p0(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg(instr & 0x3);

    uint32_t value = dsp.get_saturated_acc(ab) & 0xFFFFFFFF;

    dsp.set_product(0, value);
}

void mov_p1_ab(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg(instr & 0x3);

    uint64_t value = dsp.get_product(1);

    dsp.saturate_acc_with_flag(ab, value);
}

void mov2_px_arstep(DSP &dsp, uint16_t instr)
{
    uint32_t value = dsp.get_product_no_shift((instr >> 1) & 0x1);
    uint16_t l = value & 0xFFFF;
    uint16_t h = (value >> 16) & 0xFFFF;

    uint8_t arrn = dsp.get_arrn_unit((instr >> 8) & 0x3);
    uint8_t arstep = dsp.get_arstep((instr >> 2) & 0x3);
    uint8_t aroffset = dsp.get_aroffset((instr >> 2) & 0x3);

    uint16_t address = dsp.rn_addr_and_modify(arrn, arstep, false);
    uint16_t address2 = dsp.offset_addr(arrn, address, aroffset, false);

    dsp.write_data_word(address2, l);
    dsp.write_data_word(address, h);
}

void mov2_arstep_px(DSP &dsp, uint16_t instr)
{
    uint8_t arrn = dsp.get_arrn_unit((instr >> 10) & 0x3);
    uint8_t arstep = dsp.get_arstep((instr >> 5) & 0x3);
    uint8_t aroffset = dsp.get_aroffset((instr >> 5) & 0x3);

    uint16_t address = dsp.rn_addr_and_modify(arrn, arstep, false);
    uint16_t address2 = dsp.offset_addr(arrn, address, aroffset, false);

    uint16_t l = dsp.read_data_word(address2);
    uint16_t h = dsp.read_data_word(address);

    uint32_t value = l | (h << 16);

    dsp.set_product(instr & 0x1, value);
}

void mova_ab_arstep(DSP& dsp, uint16_t instr)
{
    DSP_REG acc = get_ab_reg((instr >> 4) & 0x3);

    uint64_t value = dsp.get_saturated_acc(acc);
    uint16_t l = value & 0xFFFF;
    uint16_t h = (value >> 16) & 0xFFFF;

    uint8_t arrn = dsp.get_arrn_unit((instr >> 2) & 0x3);
    uint8_t arstep = dsp.get_arstep(instr & 0x3);
    uint8_t aroffset = dsp.get_aroffset(instr & 0x3);

    uint16_t address = dsp.rn_addr_and_modify(arrn, arstep, false);
    uint16_t address2 = dsp.offset_addr(arrn, address, aroffset, false);

    dsp.write_data_word(address2, l);
    dsp.write_data_word(address, h);
}

void mova_arstep_ab(DSP &dsp, uint16_t instr)
{
    DSP_REG acc = get_ab_reg((instr >> 4) & 0x3);

    uint8_t arrn = dsp.get_arrn_unit((instr >> 2) & 0x3);
    uint8_t arstep = dsp.get_arstep(instr & 0x3);
    uint8_t aroffset = dsp.get_aroffset(instr & 0x3);

    uint16_t address = dsp.rn_addr_and_modify(arrn, arstep, false);
    uint16_t address2 = dsp.offset_addr(arrn, address, aroffset, false);

    uint16_t l = dsp.read_data_word(address2);
    uint16_t h = dsp.read_data_word(address);

    dsp.saturate_acc_with_flag(acc, l | (h << 16));
}

void mov_r6_reg(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    dsp.set_reg16(reg, dsp.get_reg16(DSP_REG_R6, false));
}

void mov_reg_r6(DSP &dsp, uint16_t instr)
{
    DSP_REG reg = get_register(instr & 0x1F);

    dsp.set_reg16(DSP_REG_R6, dsp.get_reg16(reg, true));
}

void movs_memimm8_ab(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 11) & 0x3);

    uint8_t imm = instr & 0xFF;

    uint64_t value = SignExtend<16, uint64_t>(dsp.read_from_page(imm));

    dsp.shift_reg_40(value, ab, dsp.get_sv());
}

void movs_rn_step_ab(DSP &dsp, uint16_t instr)
{
    uint8_t rn = instr & 0x7;
    uint8_t step = (instr >> 3) & 0x3;

    DSP_REG ab = get_ab_reg((instr >> 5) & 0x3);

    uint16_t addr = dsp.rn_addr_and_modify(rn, step, false);
    uint64_t value = SignExtend<16, uint64_t>(dsp.read_data_word(addr));

    uint16_t shift = dsp.get_sv();
    dsp.shift_reg_40(value, ab, shift);
}

void movs_reg_ab(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 5) & 0x3);
    DSP_REG reg = get_register(instr & 0x1F);

    uint64_t value = SignExtend<16, uint64_t>(dsp.get_reg16(reg, false));

    uint16_t shift = dsp.get_sv();
    dsp.shift_reg_40(value, ab, shift);
}

void movsi(DSP &dsp, uint16_t instr)
{
    DSP_REG rnold = get_rnold((instr >> 9) & 0x7);
    DSP_REG ab = get_ab_reg((instr >> 5) & 0x3);

    uint64_t value = SignExtend<16, uint64_t>(dsp.get_reg16(rnold, false));

    uint16_t imm = instr & 0x1F;
    imm = SignExtend<5>(imm);

    dsp.shift_reg_40(value, ab, imm);
}

void mov2_abh_m(DSP &dsp, uint16_t instr)
{
    DSP_REG a = get_abh_reg((instr >> 4) & 0x3);
    DSP_REG b = get_abh_reg((instr >> 2) & 0x3);

    uint16_t u = (dsp.get_saturated_acc(a, false) >> 16) & 0xFFFF;
    uint16_t v = (dsp.get_saturated_acc(b, false) >> 16) & 0xFFFF;

    uint8_t arrn = dsp.get_arrn_unit((instr >> 1) & 0x1);
    uint8_t arstep = dsp.get_arstep(instr & 0x1);
    uint8_t aroffset = dsp.get_aroffset(instr & 0x1);

    uint16_t addr_u = dsp.rn_addr_and_modify(arrn, arstep, false);
    uint16_t addr_v = dsp.offset_addr(arrn, addr_u, aroffset, false);

    dsp.write_data_word(addr_v, v);
    dsp.write_data_word(addr_u, u);
}

void exchange_jai(DSP &dsp, uint16_t instr)
{
    DSP_REG acc = get_axh_reg((instr >> 6) & 0x1);
    uint8_t ui = dsp.get_arprni((instr >> 4) & 0x3);
    uint8_t uj = dsp.get_arprnj((instr >> 4) & 0x3);

    uint8_t stepi = dsp.get_arpstepi(instr & 0x3);
    uint8_t stepj = dsp.get_arpstepj((instr >> 2) & 0x3);

    uint16_t i = dsp.rn_addr_and_modify(ui, stepi, false);
    uint16_t j = dsp.rn_addr_and_modify(uj, stepj, false);

    uint64_t value = dsp.get_acc(acc);

    dsp.write_data_word(i, (value >> 16) & 0xFFFF);

    value = SignExtend<32, uint64_t>((uint64_t)dsp.read_data_word(j) << 16);
    dsp.set_acc(acc, value);
}

void lim(DSP &dsp, uint16_t instr)
{
    DSP_REG a = get_ax_reg((instr >> 5) & 0x1);
    DSP_REG b = get_ax_reg((instr >> 4) & 0x1);

    //We call saturate directly because this instruction is unaffected by the SAT/SATA flags
    uint64_t value = dsp.saturate(dsp.get_acc(a));

    dsp.set_acc_and_flag(b, value);
}

void clrp0(DSP &dsp, uint16_t instr)
{
    dsp.set_product(0, 0);
}

void clrp1(DSP &dsp, uint16_t instr)
{
    dsp.set_product(1, 0);
}

void clrp(DSP &dsp, uint16_t instr)
{
    dsp.set_product(0, 0);
    dsp.set_product(1, 0);
}

void max_gt(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 8) & 0x1);
    DSP_REG counter = get_counter_acc(ax);

    uint64_t u = dsp.get_acc(ax);
    uint64_t v = dsp.get_acc(counter);
    uint64_t d = v - u;

    uint16_t r0 = dsp.rn_and_modify(0, (instr >> 3) & 0x3, false);

    if (((d >> 63) & 0x1) == 0 && d != 0)
    {
        dsp.set_mixp(r0);
        dsp.set_acc(ax, v);
        dsp.set_fm(true);
    }
    else
        dsp.set_fm(false);
}

void min_lt(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 8) & 0x1);
    DSP_REG counter = get_counter_acc(ax);

    uint64_t u = dsp.get_acc(ax);
    uint64_t v = dsp.get_acc(counter);
    uint64_t d = v - u;

    uint16_t r0 = dsp.rn_and_modify(0, (instr >> 3) & 0x3, false);

    if (((d >> 63) & 0x1) == 1)
    {
        dsp.set_mixp(r0);
        dsp.set_acc(ax, v);
        dsp.set_fm(true);
    }
    else
        dsp.set_fm(false);
}

void mma_emod_emod_syxx_xyxx_ac(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 6) & 0x3);

    uint8_t ui = dsp.get_arprni((instr >> 5) & 0x1);
    uint8_t uj = dsp.get_arprnj((instr >> 5) & 0x1);

    uint8_t stepi = dsp.get_arpstepi((instr >> 3) & 0x1);
    uint8_t stepj = dsp.get_arpstepj((instr >> 4) & 0x1);

    uint8_t oi = dsp.get_arpoffseti((instr >> 3) & 0x1);
    uint8_t oj = dsp.get_arpoffsetj((instr >> 4) & 0x1);

    bool sub, p0_align, p1_align, x0_sign, x1_sign, y1_sign;

    x1_sign = instr & 0x1;
    y1_sign = !x1_sign;

    sub = !((instr >> 2) & 0x1);

    if ((instr >> 8) & 0x1)
    {
        x0_sign = true;
        p0_align = false;
        p1_align = (instr >> 1) & 0x1;
    }
    else
    {
        x0_sign = false;
        p0_align = (instr >> 1) & 0x1;
        p1_align = true;
    }

    dsp.product_sum(1, ab, sub, p0_align, sub, p1_align);

    uint16_t x = dsp.rn_addr_and_modify(ui, stepi, false);
    uint16_t y = dsp.rn_addr_and_modify(uj, stepj, false);

    dsp.set_x(0, dsp.read_data_word(x));
    dsp.set_y(0, dsp.read_data_word(y));
    dsp.set_x(1, dsp.read_data_word(dsp.offset_addr(ui, x, oi, false)));
    dsp.set_y(1, dsp.read_data_word(dsp.offset_addr(uj, y, oj, false)));

    dsp.multiply(0, x0_sign, true);
    dsp.multiply(1, x1_sign, y1_sign);
}

void mma_emod_emod_sysx_sysx_zr(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 10) & 0x3);

    uint8_t ui = dsp.get_arprni((instr >> 2) & 0x1);
    uint8_t uj = dsp.get_arprnj((instr >> 2) & 0x1);

    uint8_t stepi = dsp.get_arpstepi(instr & 0x1);
    uint8_t stepj = dsp.get_arpstepj((instr >> 1) & 0x1);

    uint8_t oi = dsp.get_arpoffseti(instr & 0x1);
    uint8_t oj = dsp.get_arpoffsetj((instr >> 1) & 0x1);

    dsp.product_sum(0, ab, false, false, false, (instr >> 8) & 0x1);

    uint16_t x = dsp.rn_addr_and_modify(ui, stepi, false);
    uint16_t y = dsp.rn_addr_and_modify(uj, stepj, false);

    dsp.set_x(0, dsp.read_data_word(x));
    dsp.set_y(0, dsp.read_data_word(y));
    dsp.set_x(1, dsp.read_data_word(dsp.offset_addr(ui, x, oi, false)));
    dsp.set_y(1, dsp.read_data_word(dsp.offset_addr(uj, y, oj, false)));

    dsp.multiply(0, true, true);
    dsp.multiply(1, true, true);
}

void mma_emod_emod_sysx_sysx_ac_2(DSP &dsp, uint16_t instr)
{
    DSP_REG ab = get_ab_reg((instr >> 6) & 0x3);

    uint8_t ui = dsp.get_arprni((instr >> 4) & 0x3);
    uint8_t uj = dsp.get_arprnj((instr >> 4) & 0x3);

    uint8_t stepi = dsp.get_arpstepi(instr & 0x3);
    uint8_t stepj = dsp.get_arpstepj((instr >> 2) & 0x1);

    uint8_t oi = dsp.get_arpoffseti(instr & 0x3);
    uint8_t oj = dsp.get_arpoffsetj((instr >> 2) & 0x1);

    bool sub = (instr >> 8) & 0x1;

    dsp.product_sum(1, ab, sub, false, sub, false);

    uint16_t x = dsp.rn_addr_and_modify(ui, stepi, false);
    uint16_t y = dsp.rn_addr_and_modify(uj, stepj, false);

    dsp.set_x(0, dsp.read_data_word(x));
    dsp.set_y(0, dsp.read_data_word(y));
    dsp.set_x(1, dsp.read_data_word(dsp.offset_addr(ui, x, oi, false)));
    dsp.set_y(1, dsp.read_data_word(dsp.offset_addr(uj, y, oj, false)));

    dsp.multiply(0, true, true);
    dsp.multiply(1, true, true);
}

void mma_my_my(DSP &dsp, uint16_t instr)
{
    DSP_REG ax = get_ax_reg((instr >> 8) & 0x1);
    uint8_t arrn = dsp.get_arrn_unit((instr >> 4) & 0x1);
    uint8_t arstep = dsp.get_arstep((instr >> 3) & 0x1);
    uint8_t aroffset = dsp.get_aroffset((instr >> 3) & 0x1);

    bool x1_sign = !(instr & 0x1);
    bool p1_align = (instr >> 1) & 0x1;
    bool sub = !((instr >> 2) & 0x1);

    dsp.product_sum(1, ax, sub, false, sub, p1_align);

    uint16_t addr = dsp.rn_addr_and_modify(arrn, arstep, false);

    dsp.set_x(0, dsp.read_data_word(addr));
    dsp.set_x(1, dsp.offset_addr(arrn, addr, aroffset, false));

    dsp.multiply(0, true, true);
    dsp.multiply(1, x1_sign, true);
}

};
