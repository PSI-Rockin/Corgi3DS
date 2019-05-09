#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "gpu.hpp"
#include "bswp.hpp"

GPU::GPU()
{
    vram = nullptr;
    top_screen = nullptr;
    bottom_screen = nullptr;
}

GPU::~GPU()
{
    delete[] vram;
    delete[] top_screen;
    delete[] bottom_screen;
}

void GPU::reset()
{
    if (!vram)
        vram = new uint8_t[0x00600000];

    if (!top_screen)
        top_screen = new uint8_t[240 * 400 * 4];

    if (!bottom_screen)
        bottom_screen = new uint8_t[240 * 320 * 4];

    memset(&framebuffers, 0, sizeof(framebuffers));
    memset(memfill, 0, sizeof(memfill));
}

void GPU::render_frame()
{
    for (int y = 0; y < 400; y++)
    {
        for (int x = 0; x < 240; x++)
            render_fb_pixel(top_screen, 0, x, y);
    }

    for (int y = 0; y < 320; y++)
    {
        for (int x = 0; x < 240; x++)
            render_fb_pixel(bottom_screen, 1, x, y);
    }
}

void GPU::render_fb_pixel(uint8_t *screen, int fb_index, int x, int y)
{
    FrameBuffer* screen_fb = &framebuffers[fb_index];

    uint32_t start = screen_fb->left_addr_a % 0x00600000;

    int index = x + (y * 240);
    uint32_t color;
    switch (screen_fb->color_format)
    {
        case 0:
            color = bswp32(*(uint32_t*)&vram[start + index]);
            *(uint32_t*)&screen[index] = color;
            break;
        case 1:
            color = 0xFF000000;
            color |= vram[start + (index * 3)] << 16;
            color |= vram[start + (index * 3) + 1] << 8;
            color |= vram[start + (index * 3) + 2];
            *(uint32_t*)&screen[(index * 4)] = color;
            break;
        default:
            printf("[GPU] Unrecognized framebuffer color format %d\n", screen_fb->color_format);
            exit(1);
    }
}

uint32_t GPU::read32(uint32_t addr)
{
    addr &= 0x1FFF;
    if (addr >= 0x010 && addr < 0x030)
    {
        int index = (addr - 0x10) / 16;
        int reg = (addr / 4) & 0x3;

        switch (reg)
        {
            case 3:
                return memfill[index].finished << 1;
        }
    }
    if (addr >= 0x400 && addr < 0x500)
        return read32_fb(0, addr);
    if (addr >= 0x500 && addr < 0x600)
        return read32_fb(1, addr);
    printf("[GPU] Unrecognized read32 $%08X\n", addr);
    return 0;
}

void GPU::write32(uint32_t addr, uint32_t value)
{
    addr &= 0x1FFF;
    if (addr >= 0x010 && addr < 0x030)
    {
        int index = (addr - 0x10) / 16;
        int reg = (addr / 4) & 0x3;

        switch (reg)
        {
            case 0:
                memfill[index].start = value * 8;
                break;
            case 1:
                memfill[index].end = value * 8;
                break;
            case 2:
                memfill[index].value = value;
                break;
            case 3:
                memfill[index].finished = false;
                memfill[index].fill_width = (value >> 8) & 0x3;

                //Perform memory transfer
                if (value & 0x1)
                {
                    //TODO: Is the end region inclusive or exclusive? This code assumes exclusive
                    for (int i = memfill[index].start; i < memfill[index].end; i++)
                    {
                        switch (memfill[index].fill_width)
                        {
                            case 2:
                                write_vram<uint32_t>(i, memfill[index].value);
                                break;
                            default:
                                printf("[GPU] Unrecognized Memory Fill format %d\n", memfill[index].fill_width);
                                exit(1);
                        }
                    }
                    memfill[index].finished = true;
                }
                break;
        }
        return;
    }
    if (addr >= 0x400 && addr < 0x500)
    {
        write32_fb(0, addr, value);
        return;
    }
    if (addr >= 0x500 && addr < 0x600)
    {
        write32_fb(1, addr, value);
        return;
    }
    printf("[GPU] Unrecognized write32 $%08X: $%08X\n", addr, value);
}

uint32_t GPU::read32_fb(int index, uint32_t addr)
{
    FrameBuffer* fb = &framebuffers[index];
    addr &= 0xFF;
    switch (addr)
    {
        case 0x68:
            printf("[GPU] Read FB left addr: $%08X\n", fb->left_addr_a);
            return fb->left_addr_a;
        case 0x6C:
            return fb->left_addr_b;
        case 0x94:
            return fb->right_addr_a;
        case 0x98:
            return fb->right_addr_b;
        default:
            printf("[GPU] Unrecognized read32 fb%d addr $%08X\n", index, addr);
            return 0;
    }
}

void GPU::write32_fb(int index, uint32_t addr, uint32_t value)
{
    FrameBuffer* fb = &framebuffers[index];
    addr &= 0xFF;
    switch (addr)
    {
        case 0x68:
            fb->left_addr_a = value;
            return;
        case 0x6C:
            fb->left_addr_b = value;
            return;
        case 0x70:
            fb->color_format = value & 0x7;
            return;
        case 0x94:
            fb->right_addr_a = value;
            return;
        case 0x98:
            fb->right_addr_b = value;
            return;
        default:
            printf("[GPU] Unrecognized write32 fb addr $%08X: $%08X\n", addr, value);
    }
}

uint8_t* GPU::get_top_buffer()
{
    return top_screen;
}

uint8_t* GPU::get_bottom_buffer()
{
    return bottom_screen;
}
