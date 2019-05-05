#include <cstdio>
#include "gpu.hpp"
#include "bswp.hpp"

GPU::GPU()
{
    vram = nullptr;
}

GPU::~GPU()
{
    delete[] vram;
}

void GPU::reset()
{
    if (!vram)
        vram = new uint8_t[0x00600000];
}

uint32_t GPU::read_vram32(uint32_t addr)
{
    return *(uint32_t*)&vram[addr % 0x00600000];
}

void GPU::write_vram32(uint32_t addr, uint32_t value)
{
    //printf("[GPU] Write32 VRAM $%08X: $%08X\n", addr, value);
    *(uint32_t*)&vram[addr % 0x00600000] = bswp32(value);
}

uint32_t GPU::read32(uint32_t addr)
{
    addr &= 0x1FFF;
    if (addr >= 0x400 && addr < 0x500)
        return read32_fb(fb0, addr);
    if (addr >= 0x500 && addr < 0x600)
        return read32_fb(fb1, addr);
    printf("[GPU] Unrecognized read32 $%08X\n", addr);
    return 0;
}

void GPU::write32(uint32_t addr, uint32_t value)
{
    addr &= 0x1FFF;
    if (addr >= 0x400 && addr < 0x500)
    {
        write32_fb(fb0, addr, value);
        return;
    }
    if (addr >= 0x500 && addr < 0x600)
    {
        write32_fb(fb1, addr, value);
        return;
    }
    printf("[GPU] Unrecognized write32 $%08X: $%08X\n", addr, value);
}

uint32_t GPU::read32_fb(FrameBuffer &fb, uint32_t addr)
{
    addr &= 0xFF;
    switch (addr)
    {
        case 0x68:
            printf("[GPU] Read FB left addr: $%08X\n", fb.left_addr_a);
            return fb.left_addr_a;
        case 0x6C:
            return fb.left_addr_b;
        case 0x94:
            return fb.right_addr_a;
        case 0x98:
            return fb.right_addr_b;
        default:
            printf("[GPU] Unrecognized read32 fb addr $%08X\n", addr);
            return 0;
    }
}

void GPU::write32_fb(FrameBuffer &fb, uint32_t addr, uint32_t value)
{
    addr &= 0xFF;
    switch (addr)
    {
        case 0x68:
            fb.left_addr_a = value;
            return;
        case 0x6C:
            fb.left_addr_b = value;
            return;
        case 0x94:
            fb.right_addr_a = value;
            return;
        case 0x98:
            fb.right_addr_b = value;
            return;
        default:
            printf("[GPU] Unrecognized write32 fb addr $%08X: $%08X\n", addr, value);
    }
}

uint8_t* GPU::get_buffer()
{
    return vram;
}
