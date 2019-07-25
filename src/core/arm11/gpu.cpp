#include <cstdio>
#include <cstring>
#include "gpu.hpp"
#include "mpcore_pmr.hpp"
#include "../emulator.hpp"
#include "../scheduler.hpp"
#include "../common/common.hpp"

GPU::GPU(Emulator* e, Scheduler* scheduler, MPCore_PMR* pmr) : e(e), scheduler(scheduler), pmr(pmr)
{
    vram = nullptr;
    top_screen = nullptr;
    bottom_screen = nullptr;
}

GPU::~GPU()
{
    //vram is passed by the emulator, so no need to delete
    delete[] top_screen;
    delete[] bottom_screen;
}

void GPU::reset(uint8_t* vram)
{
    this->vram = vram;

    if (!top_screen)
        top_screen = new uint8_t[240 * 400 * 4];

    if (!bottom_screen)
        bottom_screen = new uint8_t[240 * 320 * 4];

    dma.busy = false;
    dma.finished = false;

    cmd_engine.busy = false;

    memset(&framebuffers, 0, sizeof(framebuffers));
    memset(memfill, 0, sizeof(memfill));
    memset(top_screen, 0, 240 * 400 * 4);
    memset(bottom_screen, 0, 240 * 320 * 4);
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

    uint32_t start;
    if (screen_fb->buffer_select)
        start = screen_fb->left_addr_b % 0x00600000;
    else
        start = screen_fb->left_addr_a % 0x00600000;

    int index = x + (y * 240);
    uint32_t color = 0xFF000000;
    switch (screen_fb->color_format)
    {
        case 0:
            color = bswp32(*(uint32_t*)&vram[start + (index * 4)]);
            break;
        case 1:
            color |= vram[start + (index * 3)] << 16;
            color |= vram[start + (index * 3) + 1] << 8;
            color |= vram[start + (index * 3) + 2];
            break;
        case 2:
        {
            uint16_t cin = *(uint16_t*)&vram[start + (index * 2)];
            uint32_t pr = ((cin >> 11) & 0x1F) << 3; //byte-aligned pixel red
            uint32_t pb = ((cin >>  0) & 0x1F) << 3; //byte-aligned pixel blue
            uint32_t pg = ((cin >>  5) & 0x3F) << 2; //byte-aligned pixel green

            color |= (pb << 16) | (pg << 8) | pr;

        }
            break;
        default:
            EmuException::die("[GPU] Unrecognized framebuffer color format %d\n", screen_fb->color_format);
    }
    *(uint32_t*)&screen[(index * 4)] = color;
}

void GPU::do_transfer_engine_dma(uint64_t param)
{
    if (dma.flags & (1 << 3))
    {
        printf("[GPU] Doing TexCopy\n");

        for (unsigned int i = 0; i < dma.tc_size; i++)
        {
            uint8_t data = e->arm11_read8(0, dma.input_addr);
            e->arm11_write8(0, dma.output_addr, data);

            dma.input_addr++;
            dma.output_addr++;
        }
    }
    else
    {
        printf("[GPU] Doing DisplayCopy\n");
    }
    pmr->assert_hw_irq(0x2C);
    dma.busy = false;
    dma.finished = true;
}

void GPU::do_command_engine_dma(uint64_t unused)
{
    printf("[GPU] Doing command engine DMA\n");
    printf("[GPU] Addr: $%08X Words: $%08X\n", cmd_engine.input_addr, cmd_engine.size);
    //NOTE: Here, size is in units of words
    while (cmd_engine.size)
    {
        uint32_t param = e->arm11_read32(0, cmd_engine.input_addr);
        uint32_t cmd_header = e->arm11_read32(0, cmd_engine.input_addr + 4);
        cmd_engine.input_addr += 8;
        cmd_engine.size -= 2;

        uint16_t cmd_id = cmd_header & 0xFFFF;
        uint8_t param_mask = (cmd_header >> 16) & 0xF;
        uint8_t extra_param_count = (cmd_header >> 20) & 0xFF;
        bool consecutive_writes = cmd_header >> 31;

        uint32_t extra_params[256];

        for (unsigned int i = 0; i < extra_param_count; i++)
        {
            extra_params[i] = e->arm11_read32(0, cmd_engine.input_addr);
            cmd_engine.input_addr += 4;
            cmd_engine.size--;
        }

        //Keep the command buffer 8-byte aligned
        if (extra_param_count & 0x1)
        {
            cmd_engine.input_addr += 4;
            cmd_engine.size--;
        }

        printf("[GPU] [$%08X] CMD $%02X - Mask: $%02X Consecutive: %d Extra params: %d\n",
               cmd_header, cmd_id, param_mask, consecutive_writes, extra_param_count);
        printf("[GPU] Param: $%08X\n", param);

        for (unsigned int i = 0; i < extra_param_count; i++)
            printf("[GPU] Ex param%d: $%08X\n", i, extra_params[i]);

        switch (cmd_id)
        {
            case 0x010:
                //End of command list
                pmr->assert_hw_irq(0x2D);
                break;
            default:
                printf("[GPU] Unrecognized command $%04X\n", cmd_id);
                break;
        }
    }

    cmd_engine.busy = false;
}

void GPU::do_memfill(int index)
{
    //TODO: Is the end region inclusive or exclusive? This code assumes exclusive
    for (uint32_t i = memfill[index].start; i < memfill[index].end; i++)
    {
        switch (memfill[index].fill_width)
        {
            case 2:
                write_vram<uint32_t>(i, memfill[index].value);
                break;
            default:
                EmuException::die("[GPU] Unrecognized Memory Fill format %d\n", memfill[index].fill_width);
        }
    }
    memfill[index].finished = true;
    memfill[index].busy = false;

    //Memfill complete interrupt
    pmr->assert_hw_irq(0x28 + index);
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
            {
                uint32_t value = memfill[index].busy;
                value |= memfill[index].finished << 1;
                return value;
            }
        }
    }
    if (addr >= 0x400 && addr < 0x500)
        return read32_fb(0, addr);
    if (addr >= 0x500 && addr < 0x600)
        return read32_fb(1, addr);
    switch (addr)
    {
        case 0x0C18:
            return dma.busy | (dma.finished << 8);
        case 0x18F0:
            return cmd_engine.busy;
    }
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
                    printf("[GPU] Start memfill%d\n", index);
                    memfill[index].busy = true;

                    //TODO: How long does a memfill take? We just assume a constant value for now
                    scheduler->add_event([this](uint64_t param) { this->do_memfill(param);}, 1000, index);
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
    switch (addr)
    {
        case 0x0C00:
            dma.input_addr = value << 3;
            printf("[GPU] DMA input addr: $%08X\n", dma.input_addr);
            break;
        case 0x0C04:
            dma.output_addr = value << 3;
            printf("[GPU] DMA output addr: $%08X\n", dma.output_addr);
            break;
        case 0x0C10:
            dma.flags = value;
            printf("[GPU] DMA flags: $%08X\n", dma.flags);
            break;
        case 0x0C18:
            if (value & 0x1)
            {
                dma.busy = true;
                dma.finished = false;
                scheduler->add_event([this](uint64_t param) { this->do_transfer_engine_dma(param);}, 1000);
            }
            break;
        case 0x0C20:
            dma.tc_size = value;
            printf("[GPU] TexCopy size: $%08X\n", dma.tc_size);
            break;
        case 0x0C24:
            dma.tc_input_width = (value & 0xFFFF) * 16;
            dma.tc_input_gap = (value >> 16) * 16;
            printf("[GPU] TexCopy input width/gap: $%08X $%08X\n", dma.tc_input_width, dma.tc_input_gap);
            break;
        case 0x0C28:
            dma.tc_output_width = (value & 0xFFFF) * 16;
            dma.tc_output_gap = (value >> 16) * 16;
            printf("[GPU] TexCopy output width/gap: $%08X $%08X\n", dma.tc_output_width, dma.tc_output_gap);
            break;
        case 0x18E0:
            //Here, size is in units of words
            cmd_engine.size = value << 1;
            break;
        case 0x18E8:
            //Addr is in bytes, however
            cmd_engine.input_addr = value << 3;
            break;
        case 0x18F0:
            //P3D IRQ
            if (value & 0x1)
            {
                cmd_engine.busy = true;
                scheduler->add_event([this](uint64_t param) { this->do_command_engine_dma(param);}, 1000);
            }
            break;
        default:
            printf("[GPU] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}

uint32_t GPU::read32_fb(int index, uint32_t addr)
{
    FrameBuffer* fb = &framebuffers[index];
    addr &= 0xFF;
    switch (addr)
    {
        case 0x68:
            return fb->left_addr_a;
        case 0x6C:
            return fb->left_addr_b;
        case 0x78:
            return fb->buffer_select;
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
            printf("[GPU] Write fb%d left addr A: $%08X\n", index, value);
            fb->left_addr_a = value;
            return;
        case 0x6C:
            printf("[GPU] Write fb%d left addr B: $%08X\n", index, value);
            fb->left_addr_b = value;
            return;
        case 0x70:
            printf("[GPU] Write fb%d color format: $%08X\n", index, value);
            fb->color_format = value & 0x7;
            return;
        case 0x78:
            printf("[GPU] Write fb%d buffer select: $%08X\n", index, value);
            fb->buffer_select = value & 0x1;
            return;
        case 0x94:
            printf("[GPU] Write fb%d right addr A: $%08X\n", index, value);
            fb->right_addr_a = value;
            return;
        case 0x98:
            printf("[GPU] Write fb%d right addr B: $%08X\n", index, value);
            fb->right_addr_b = value;
            return;
        default:
            printf("[GPU] Unrecognized write32 fb%d addr $%08X: $%08X\n", index, addr, value);
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
