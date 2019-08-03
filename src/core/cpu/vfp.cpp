#include "../common/common.hpp"
#include "vfp.hpp"

VFP::VFP()
{

}

uint32_t VFP::get_fpexc()
{
    return fpexc;
}

uint32_t VFP::get_fpscr()
{
    uint32_t reg = 0;
    reg |= fpscr.vector_len << 16;
    reg |= fpscr.vector_stride << 20;
    reg |= fpscr.round_mode << 22;
    reg |= fpscr.overflow << 28;
    reg |= fpscr.carry << 29;
    reg |= fpscr.zero << 30;
    reg |= fpscr.negative << 31;
    //printf("[VFP] Read FPSCR: $%08X\n", reg);
    return reg;
}

void VFP::set_fpexc(uint32_t value)
{
    fpexc = value;
}

void VFP::set_fpscr(uint32_t value)
{
    printf("[VFP] Write FPSCR: $%08X\n", value);

    fpscr.vector_len = (value >> 16) & 0x7;
    fpscr.vector_stride = (value >> 20) & 0x3;
    fpscr.round_mode = (value >> 22) & 0x3;
    fpscr.overflow = value & (1 << 28);
    fpscr.carry = value & (1 << 29);
    fpscr.zero = value & (1 << 30);
    fpscr.negative = value & (1 << 31);

    if (fpscr.vector_len > 1 || fpscr.vector_stride)
    {
        EmuException::die("[VFP] Vector mode unimplemented!");
    }
}
