#include "../common/common.hpp"
#include "arm.hpp"
#include "arm_interpret.hpp"
#include "vfp.hpp"

namespace ARM_Interpreter
{

void vfp_load_store(ARM_CPU& cpu, VFP &vfp, uint32_t instr)
{
    bool p = (instr >> 24) & 0x1;
    bool u = (instr >> 23) & 0x1;
    bool w = (instr >> 21) & 0x1;
    bool l = (instr >> 20) & 0x1;

    uint16_t op = (p << 3) | (u << 2) | (w << 1) | l;

    switch (op)
    {
        case 0x06:
            return vfp_store_block_double(cpu, vfp, instr);
        default:
            //EmuException::die("[VFP_Interpreter] Undefined load/store instr $%04X\n", op);
            printf("[VFP_Interpreter] Undefined load/store instr $%04X\n", op);
    }
}

void vfp_store_block_double(ARM_CPU& cpu, VFP &vfp, uint32_t instr)
{
    bool increment = (instr >> 23) & 0x1;
    bool writeback = (instr >> 21) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t list_start = (instr >> 12) & 0xF;
    uint32_t offset = (instr & 0xFF) / 2;

    uint32_t addr = cpu.get_register(base);

    if (!increment)
        EmuException::die("FSTMDB is a blorp");

    for (unsigned int i = list_start; i < list_start + offset; i++)
    {
        cpu.write32(addr, 0);
        cpu.write32(addr + 4, 0);
        addr += 8;
    }

    if (writeback)
        cpu.set_register(base, addr);
    printf("FSTM unimplemented!!!\n");
}

};
