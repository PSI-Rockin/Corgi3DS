#include <algorithm>
#include <cstdio>
#include <cstring>
#include "gpu.hpp"
#include "signextend.hpp"
#include "mpcore_pmr.hpp"
#include "../emulator.hpp"
#include "../scheduler.hpp"
#include "../common/common.hpp"

// 8x8 Z-Order coordinate from 2D coordinates
// https://github.com/citra-emu/citra/blob/aca55d0378f1fadb73f1ad54826f19e029a0674f/src/video_core/utils.h#L12
static constexpr uint32_t MortonInterleave(uint32_t x, uint32_t y)
{
    constexpr uint32_t xlut[] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15};
    constexpr uint32_t ylut[] = {0x00, 0x02, 0x08, 0x0a, 0x20, 0x22, 0x28, 0x2a};
    return xlut[x % 8] + ylut[y % 8];
}

// https://github.com/citra-emu/citra/blob/master/src/common/color.h
// Convert a 1-bit color component to 8 bit
constexpr uint32_t Convert1To8(uint32_t value) {
    return value * 255;
}

// Convert a 4-bit color component to 8 bit
constexpr uint32_t Convert4To8(uint32_t value) {
    return (value << 4) | value;
}

// Convert a 5-bit color component to 8 bit
constexpr uint32_t Convert5To8(uint32_t value) {
    return (value << 3) | (value >> 2);
}

// Convert a 6-bit color component to 8 bit
constexpr uint32_t Convert6To8(uint32_t value) {
    return (value << 2) | (value >> 4);
}

// Convert a 8-bit color component to 1 bit
constexpr uint32_t Convert8To1(uint32_t value) {
    return value >> 7;
}

// Convert a 8-bit color component to 4 bit
constexpr uint32_t Convert8To4(uint32_t value) {
    return value >> 4;
}

// Convert a 8-bit color component to 5 bit
constexpr uint32_t Convert8To5(uint32_t value) {
    return value >> 3;
}

// Convert a 8-bit color component to 6 bit
constexpr uint32_t Convert8To6(uint32_t value) {
    return value >> 2;
}

float24 dp4(Vec4<float24> a, Vec4<float24> b)
{
    float24 dp = float24::Zero();

    for (int i = 0; i < 4; i++)
        dp += a[i] * b[i];

    return dp;
}

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

    lcd_initialized = false;

    framebuffers[0].left_addr_a = 0x18000000;
    framebuffers[1].left_addr_a = 0x18000000;

    memset(ctx.texcomb_rgb_buffer_update, 0, sizeof(ctx.texcomb_rgb_buffer_update));
    memset(ctx.texcomb_alpha_buffer_update, 0, sizeof(ctx.texcomb_alpha_buffer_update));
}

void GPU::render_frame()
{
    for (int y = 0; y < 400; y++)
    {
        for (int x = 0; x < 240; x++)
        {
            if (!framebuffers[0].screenfill_enabled && lcd_initialized)
                render_fb_pixel(top_screen, 0, x, y);
            else
                *(uint32_t*)&top_screen[(x + (y * 240)) * 4] = 0xFF000000 | framebuffers[0].screenfill_color;
        }
    }

    for (int y = 0; y < 320; y++)
    {
        for (int x = 0; x < 240; x++)
        {
            if (!framebuffers[1].screenfill_enabled && lcd_initialized)
                render_fb_pixel(bottom_screen, 1, x, y);
            else
                *(uint32_t*)&bottom_screen[(x + (y * 240)) * 4] = 0xFF000000 | framebuffers[1].screenfill_color;
        }
    }
}

void GPU::render_fb_pixel(uint8_t *screen, int fb_index, int x, int y)
{
    FrameBuffer* screen_fb = &framebuffers[fb_index];

    uint32_t start;
    if (screen_fb->buffer_select)
        start = screen_fb->left_addr_b;
    else
        start = screen_fb->left_addr_a;

    uint32_t screen_pos = (x + (y * 240)) * 4;
    y *= screen_fb->stride;
    uint32_t color = 0xFF000000;
    switch (screen_fb->color_format)
    {
        case 0:
            x *= 4;
            color = bswp32(e->arm11_read32(0, start + x + y));
            break;
        case 1:
            x *= 3;
            color |= e->arm11_read8(0, start + x + y) << 16;
            color |= e->arm11_read8(0, start + x + y + 1) << 8;
            color |= e->arm11_read8(0, start + x + y + 2);
            break;
        case 2:
        {
            x *= 2;
            uint16_t cin = e->arm11_read16(0, start + x + y);
            uint32_t pr = Convert5To8((cin >> 11) & 0x1F); //byte-aligned pixel red
            uint32_t pb = Convert5To8((cin >>  0) & 0x1F); //byte-aligned pixel blue
            uint32_t pg = Convert6To8((cin >>  5) & 0x3F); //byte-aligned pixel green

            color |= (pb << 16) | (pg << 8) | pr;
        }
            break;
        case 4:
        {
            x *= 2;
            uint16_t cin = e->arm11_read16(0, start + x + y);
            color = Convert4To8(cin >> 12);
            color |= Convert4To8((cin >> 8) & 0xF) << 8;
            color |= Convert4To8((cin >> 4) & 0xF) << 16;
            color |= Convert4To8(cin & 0xF) << 24;
        }
            break;
        default:
            EmuException::die("[GPU] Unrecognized framebuffer color format %d\n", screen_fb->color_format);
    }
    *(uint32_t*)&screen[screen_pos] = color;
}

uint32_t GPU::get_4bit_swizzled_addr(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t offs = ((x & ~0x7) * 8) + ((y & ~0x7) * width);
    offs += MortonInterleave(x, y);
    return base + (offs >> 1);
}

uint32_t GPU::get_swizzled_tile_addr(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t size)
{
    uint32_t offs = ((x & ~0x7) * 8) + ((y & ~0x7) * width);
    offs += MortonInterleave(x, y);
    return base + (offs * size);
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

        printf("Input addr: $%08X Output addr: $%08X\n", dma.input_addr, dma.output_addr);
        printf("Input width/height: %d %d\n", dma.disp_input_width, dma.disp_input_height);
        printf("Output width/height: %d %d\n", dma.disp_output_width, dma.disp_output_height);
        printf("Flags: $%08X\n", dma.flags);

        uint32_t input_addr = dma.input_addr;
        uint32_t output_addr = dma.output_addr;

        uint32_t input_size = 1;
        uint32_t output_size = 1;

        uint8_t input_format = (dma.flags >> 8) & 0x7;
        uint8_t output_format = (dma.flags >> 12) & 0x7;

        const static int format_sizes[] = {4, 3, 2, 2, 2, 0, 0};

        input_size *= format_sizes[input_format];
        output_size *= format_sizes[output_format];

        bool linear_to_tiled = (dma.flags >> 1) & 0x1;

        int input_y;
        if (dma.disp_input_height > dma.disp_output_height)
        {
            //TODO: is this right?
            input_y = dma.disp_output_height - dma.disp_input_height;
            if (dma.flags & 0x1)
                input_y = dma.disp_input_height - input_y - 1;
        }
        else
        {
            input_y = 0;
            if (dma.flags & 0x1)
                input_y = dma.disp_input_height - 1;
        }

        if (dma.flags & (1 << 24))
        {
            dma.disp_input_width >>= 1;
            dma.disp_output_width >>= 1;
        }

        for (unsigned int y = 0; y < dma.disp_output_height; y++)
        {
            printf("input y: %d output y: %d\n", input_y, y);
            int input_x = 0;
            for (unsigned int x = 0; x < dma.disp_output_width; x++)
            {
                uint32_t color = 0;

                if (!linear_to_tiled)
                    input_addr = get_swizzled_tile_addr(
                            dma.input_addr, dma.disp_input_width, input_x, input_y, input_size);
                else
                    input_addr = dma.input_addr + (input_x + (input_y * dma.disp_input_width)) * input_size;

                switch (input_format)
                {
                    case 0:
                        color = bswp32(e->arm11_read32(0, input_addr));
                        break;
                    case 1:
                    {
                        color = e->arm11_read32(0, input_addr);

                        uint8_t pr = (color >> 16) & 0xFF;
                        uint8_t pg = (color >> 8) & 0xFF;
                        uint8_t pb = color & 0xFF;

                        color = (pb << 16) | (pg << 8) | pr;
                    }
                        break;
                    case 2:
                    {
                        color = e->arm11_read16(0, input_addr);
                        uint32_t pr = Convert5To8((color >> 11) & 0x1F); //byte-aligned pixel red
                        uint32_t pb = Convert5To8((color >>  0) & 0x1F); //byte-aligned pixel blue
                        uint32_t pg = Convert6To8((color >>  5) & 0x3F); //byte-aligned pixel green

                        color = (pb << 16) | (pg << 8) | pr;
                        break;
                    }
                    case 3:
                    {
                        color = e->arm11_read16(0, input_addr);

                        uint8_t r = Convert5To8((color >> 11) & 0x1F);
                        uint8_t g = Convert5To8((color >> 6) & 0x1F);
                        uint8_t b = Convert5To8((color >> 1) & 0x1F);
                        uint8_t a = Convert1To8(color & 0x1);

                        color = r | (g << 8) | (b << 16) | (a << 24);
                    }
                        break;
                    case 4:
                    {
                        color = e->arm11_read16(0, input_addr);

                        uint8_t r = Convert4To8(color >> 12);
                        uint8_t g = Convert4To8((color >> 8) & 0xF);
                        uint8_t b = Convert4To8((color >> 4) & 0xF);
                        uint8_t a = Convert4To8(color & 0xF);

                        color = r | (g << 8) | (b << 16) | (a << 24);
                    }
                        break;
                    default:
                        EmuException::die("[GPU] Unrecognized input format %d\n", input_format);
                }

                uint8_t r = color & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = (color >> 16) & 0xFF;
                uint8_t a = color >> 24;

                if (!linear_to_tiled)
                {
                    uint32_t output_offs = x + (y * dma.disp_output_width);

                    output_addr = dma.output_addr + (output_offs * output_size);
                }
                else
                {
                    uint32_t output_x = x;
                    uint32_t output_y = y;

                    output_addr = get_swizzled_tile_addr(dma.output_addr, dma.disp_output_width,
                                                         output_x, output_y, output_size);
                }

                switch (output_format)
                {
                    case 0:
                        e->arm11_write32(0, output_addr, bswp32(r | (g << 8) | (b << 16) | (a << 24)));
                        break;
                    case 1:
                    {
                        e->arm11_write8(0, output_addr, b);
                        e->arm11_write8(0, output_addr + 1, g);
                        e->arm11_write8(0, output_addr + 2, r);
                    }
                        break;
                    case 2:
                        color = 0;
                        color |= Convert8To5(b);
                        color |= Convert8To6(g) << 5;
                        color |= Convert8To5(r) << 11;
                        e->arm11_write16(0, output_addr, color);
                        break;
                    case 3:
                        color = 0;
                        color |= Convert8To1(a);
                        color |= Convert8To5(b) << 1;
                        color |= Convert8To5(g) << 6;
                        color |= Convert8To5(r) << 11;
                        e->arm11_write16(0, output_addr, color);
                        break;
                    case 4:
                        color = 0;
                        color |= Convert8To4(a);
                        color |= Convert8To4(b) << 4;
                        color |= Convert8To4(g) << 8;
                        color |= Convert8To4(r) << 12;

                        e->arm11_write16(0, output_addr, color);
                        break;
                    default:
                        EmuException::die("[GPU] Unrecognized output format %d\n", output_format);
                }

                input_x = x;

                if (dma.flags & (1 << 24))
                    input_x <<= 1;
            }

            if (dma.flags & 0x1)
                input_y--;
            else
                input_y++;
        }
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

        write_cmd_register(cmd_id, param, param_mask);

        for (unsigned int i = 0; i < extra_param_count; i++)
        {
            if (consecutive_writes)
                cmd_id++;

            write_cmd_register(cmd_id, extra_params[i], param_mask);
        }
    }

    cmd_engine.busy = false;
}

void GPU::do_memfill(int index)
{
    //TODO: Is the end region inclusive or exclusive? This code assumes exclusive
    printf("[GPU] Do memfill%d\n", index);
    printf("Start: $%08X End: $%08X Value: $%08X Width: %d\n",
           memfill[index].start, memfill[index].end, memfill[index].value, memfill[index].fill_width);
    for (uint32_t i = memfill[index].start; i < memfill[index].end; i++)
    {
        switch (memfill[index].fill_width)
        {
            case 0:
                write_vram<uint16_t>(i, memfill[index].value & 0xFFFF);
                i++;
                break;
            case 1:
                write_vram<uint8_t>(i, memfill[index].value & 0xFF);
                write_vram<uint8_t>(i + 1, (memfill[index].value >> 8) & 0xFF);
                write_vram<uint8_t>(i + 2, (memfill[index].value >> 16) & 0xFF);
                i += 2;
                break;
            case 2:
                write_vram<uint32_t>(i, memfill[index].value);
                i += 3;
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

void GPU::write_cmd_register(int reg, uint32_t param, uint8_t mask)
{
    if (reg >= 0x300)
        EmuException::die("[GPU] Command $%04X is higher than allowed!", reg);

    //Mask the param
    uint32_t real_mask = 0;
    for (int i = 0; i < 4; i++)
        real_mask |= (mask & (1 << i)) ? (0xFF << (i * 8)) : 0;

    ctx.regs[reg] &= ~real_mask;
    ctx.regs[reg] |= param & real_mask;

    param = ctx.regs[reg];

    printf("[GPU] Write command $%04X ($%08X)\n", reg, param);

    //Texture combiner regs
    if ((reg >= 0x0C0 && reg < 0x0E0) || (reg >= 0x0F0 && reg < 0x0FD))
    {
        int index = 0;
        int texcomb_reg = 0;
        if (reg < 0x0E0)
        {
            index = (reg - 0x0C0) / 8;
            texcomb_reg = (reg - 0x0C0) % 8;
        }
        else
        {
            index = ((reg - 0x0F0) / 8) + 4;
            texcomb_reg = (reg - 0x0F0) % 8;
        }

        switch (texcomb_reg)
        {
            case 0:
                ctx.texcomb_rgb_source[index][0] = param & 0xF;
                ctx.texcomb_rgb_source[index][1] = (param >> 4) & 0xF;
                ctx.texcomb_rgb_source[index][2] = (param >> 8) & 0xF;

                ctx.texcomb_alpha_source[index][0] = (param >> 16) & 0xF;
                ctx.texcomb_alpha_source[index][1] = (param >> 20) & 0xF;
                ctx.texcomb_alpha_source[index][2] = (param >> 24) & 0xF;
                break;
            case 1:
                ctx.texcomb_rgb_operand[index][0] = param & 0xF;
                ctx.texcomb_rgb_operand[index][1] = (param >> 4) & 0xF;
                ctx.texcomb_rgb_operand[index][2] = (param >> 8) & 0xF;

                ctx.texcomb_alpha_operand[index][0] = (param >> 12) & 0xF;
                ctx.texcomb_alpha_operand[index][1] = (param >> 16) & 0xF;
                ctx.texcomb_alpha_operand[index][2] = (param >> 20) & 0xF;
                break;
            case 2:
                ctx.texcomb_rgb_op[index] = param & 0xF;
                ctx.texcomb_alpha_op[index] = (param >> 16) & 0xF;
                break;
            case 3:
                ctx.texcomb_const[index].r = param & 0xFF;
                ctx.texcomb_const[index].g = (param >> 8) & 0xFF;
                ctx.texcomb_const[index].b = (param >> 16) & 0xFF;
                ctx.texcomb_const[index].a = param >> 24;
                break;
            case 4:
                ctx.texcomb_rgb_scale[index] = param & 0x3;
                ctx.texcomb_alpha_scale[index] = (param >> 16) & 0x3;
                break;
        }

        return;
    }

    //Attribute buffer regs
    if (reg >= 0x203 && reg < 0x227)
    {
        int index = (reg - 0x203) / 3;
        int attr_reg = (reg - 0x203) % 3;

        switch (attr_reg)
        {
            case 0:
                ctx.attr_buffer_offs[index] = param & 0x0FFFFFFF;
                break;
            case 1:
                ctx.attr_buffer_cfg1[index] = param;
                break;
            case 2:
                ctx.attr_buffer_cfg2[index] = param & 0xFFFF;
                ctx.attr_buffer_vtx_size[index] = (param >> 16) & 0xFF;
                ctx.attr_buffer_components[index] = param >> 28;
                break;
        }
        return;
    }

    //Fixed attr writes
    if (reg >= 0x233 && reg < 0x236)
    {
        ctx.fixed_attr_buffer[ctx.fixed_attr_count] = param;
        ctx.fixed_attr_count++;

        if (ctx.fixed_attr_count == 3)
        {
            ctx.fixed_attr_count = 0;

            Vec4<float24> attr;

            attr.w = float24::FromRaw(ctx.fixed_attr_buffer[0] >> 8);
            attr.z = float24::FromRaw(((ctx.fixed_attr_buffer[0] & 0xFF) << 16) |
                                                       ((ctx.fixed_attr_buffer[1] >> 16) & 0xFFFF));
            attr.y = float24::FromRaw(((ctx.fixed_attr_buffer[1] & 0xFFFF) << 8) |
                                                       ((ctx.fixed_attr_buffer[2] >> 24) & 0xFF));
            attr.x = float24::FromRaw(ctx.fixed_attr_buffer[2] & 0xFFFFFF);

            if (ctx.fixed_attr_index < 15)
            {
                ctx.vsh.input_attrs[ctx.fixed_attr_index] = attr;
                ctx.fixed_attr_index++;
            }
            else
            {
                ctx.vsh.input_attrs[ctx.vsh_input_counter] = attr;
                ctx.vsh_input_counter++;

                if (ctx.vsh_input_counter == ctx.vsh_inputs)
                {
                    input_vsh_vtx();
                    ctx.vsh_input_counter = 0;
                }
            }
        }
        return;
    }

    //Geometry Shader float uniform upload
    if (reg >= 0x291 && reg < 0x298)
    {
        input_float_uniform(ctx.gsh, param);
        return;
    }

    //Geometry Shader code upload
    if (reg >= 0x29C && reg < 0x2A4)
    {
        *(uint32_t*)&ctx.gsh.code[ctx.gsh.code_index] = param;
        ctx.gsh.code_index += 4;
        return;
    }

    //Geometry Shader operand descriptor upload
    if (reg >= 0x2A6 && reg < 0x2AE)
    {
        ctx.gsh.op_desc[ctx.gsh.op_desc_index] = param;
        ctx.gsh.op_desc_index++;
        return;
    }

    //Vertex Shader float uniform upload
    if (reg >= 0x2C1 && reg < 0x2C9)
    {
        input_float_uniform(ctx.vsh, param);
        return;
    }

    //Vertex Shader code upload
    if (reg >= 0x2CC && reg < 0x2D4)
    {
        *(uint32_t*)&ctx.vsh.code[ctx.vsh.code_index] = param;
        ctx.vsh.code_index += 4;
        return;
    }

    //Vertex Shader operand descriptor upload
    if (reg >= 0x2D6 && reg < 0x2DE)
    {
        ctx.vsh.op_desc[ctx.vsh.op_desc_index] = param;
        ctx.vsh.op_desc_index++;
        return;
    }

    switch (reg)
    {
        case 0x010:
            //End of command list interrupt
            pmr->assert_hw_irq(0x2D);
            break;
        case 0x040:
            ctx.cull_mode = param & 0x3;
            break;
        case 0x041:
            ctx.viewport_width = float24::FromRaw(param);
            break;
        case 0x042:
            ctx.viewport_invw = float24::FromFloat32(*(float*)&param);
            break;
        case 0x043:
            ctx.viewport_height = float24::FromRaw(param);
            break;
        case 0x044:
            ctx.viewport_invh = float24::FromFloat32(*(float*)&param);
            break;
        case 0x04D:
            ctx.depth_scale = float24::FromRaw(param);
            break;
        case 0x04E:
            ctx.depth_offset = float24::FromRaw(param);
            break;
        case 0x04F:
            ctx.sh_output_total = param & 0x7;
            break;
        case 0x050:
        case 0x051:
        case 0x052:
        case 0x053:
        case 0x054:
        case 0x055:
        case 0x056:
        {
            int index = reg - 0x050;

            ctx.sh_output_mapping[index][0] = param & 0x1F;
            ctx.sh_output_mapping[index][1] = (param >> 8) & 0x1F;
            ctx.sh_output_mapping[index][2] = (param >> 16) & 0x1F;
            ctx.sh_output_mapping[index][3] = (param >> 24) & 0x1F;
        }
            break;
        case 0x068:
            ctx.viewport_x = SignExtend<10>(param & 0x3FF);
            ctx.viewport_y = SignExtend<10>((param >> 16) & 0x3FF);
            break;
        case 0x06D:
            ctx.use_z_for_depth = param & 0x1;
            break;
        case 0x080:
            ctx.tex_enable[0] = param & 0x1;
            ctx.tex_enable[1] = (param >> 1) & 0x1;
            ctx.tex_enable[2] = (param >> 2) & 0x1;
            ctx.tex3_coords = (param >> 8) & 0x3;
            ctx.tex_enable[3] = (param >> 10) & 0x1;
            ctx.tex2_uses_tex1_coords = (param >> 13) & 0x1;
            break;
        case 0x081:
            ctx.tex_border[0].r = param & 0xFF;
            ctx.tex_border[0].g = (param >> 8) & 0xFF;
            ctx.tex_border[0].b = (param >> 16) & 0xFF;
            ctx.tex_border[0].a = param >> 24;
            break;
        case 0x082:
            ctx.tex_height[0] = param & 0x7FF;
            ctx.tex_width[0] = (param >> 16) & 0x7FF;
            break;
        case 0x083:
            ctx.tex_twrap[0] = (param >> 8) & 0x7;
            ctx.tex_swrap[0] = (param >> 12) & 0x7;
            ctx.tex0_type = (param >> 28) & 0x7;
            break;
        case 0x085:
            ctx.tex_addr[0] = (param & 0x0FFFFFFF) << 3;
            break;
        case 0x086:
        case 0x087:
        case 0x088:
        case 0x089:
        case 0x08A:
            ctx.tex0_addr[reg - 0x086] = (param & 0x0FFFFFFF) << 3;
            break;
        case 0x08E:
            ctx.tex_type[0] = param & 0xF;
            break;
        case 0x091:
        case 0x099:
            ctx.tex_border[((reg - 0x091) / 8) + 1].r = param & 0xFF;
            ctx.tex_border[((reg - 0x091) / 8) + 1].g = (param >> 8) & 0xFF;
            ctx.tex_border[((reg - 0x091) / 8) + 1].b = (param >> 16) & 0xFF;
            ctx.tex_border[((reg - 0x091) / 8) + 1].a = param >> 24;
            break;
        case 0x092:
        case 0x09A:
            ctx.tex_height[((reg - 0x092) / 8) + 1] = param & 0x7FF;
            ctx.tex_width[((reg - 0x092) / 8) + 1] = (param >> 16) & 0x7FF;
            break;
        case 0x093:
        case 0x09B:
            ctx.tex_twrap[((reg - 0x093) / 8) + 1] = (param >> 8) & 0x7;
            ctx.tex_swrap[((reg - 0x093) / 8) + 1] = (param >> 12) & 0x7;
            break;
        case 0x095:
        case 0x09D:
            ctx.tex_addr[((reg - 0x095) / 8) + 1] = (param & 0x0FFFFFFF) << 3;
            break;
        case 0x096:
        case 0x09E:
            ctx.tex_type[((reg - 0x096) / 8) + 1] = param & 0xF;
            break;
        case 0x0E0:
            //The buffer update only applies to the first four stages
            for (int i = 0; i < 4; i++)
            {
                ctx.texcomb_rgb_buffer_update[i] = (param >> (i + 8)) & 0x1;
                ctx.texcomb_alpha_buffer_update[i] = (param >> (i + 12)) & 0x1;
            }
            break;
        case 0x0FD:
            ctx.texcomb_buffer.r = param & 0xFF;
            ctx.texcomb_buffer.g = (param >> 8) & 0xFF;
            ctx.texcomb_buffer.b = (param >> 16) & 0xFF;
            ctx.texcomb_buffer.a = param >> 24;
            break;
        case 0x100:
            ctx.fragment_op = param & 0x3;
            ctx.blend_mode = (param >> 8) & 0x1;
            break;
        case 0x101:
            ctx.blend_rgb_equation = param & 0x7;
            ctx.blend_alpha_equation = (param >> 8) & 0x7;
            ctx.blend_rgb_src_func = (param >> 16) & 0xF;
            ctx.blend_rgb_dst_func = (param >> 20) & 0xF;
            ctx.blend_alpha_src_func = (param >> 24) & 0xF;
            ctx.blend_alpha_dst_func = param >> 28;
            break;
        case 0x102:
            ctx.logic_op = param & 0xF;
            break;
        case 0x103:
            ctx.blend_color.r = param & 0xFF;
            ctx.blend_color.g = (param >> 8) & 0xFF;
            ctx.blend_color.b = (param >> 16) & 0xFF;
            ctx.blend_color.a = param >> 24;
            break;
        case 0x104:
            ctx.alpha_test_enabled = param & 0x1;
            ctx.alpha_test_func = (param >> 4) & 0x7;
            ctx.alpha_test_ref = (param >> 8) & 0xFF;
            break;
        case 0x105:
            ctx.stencil_test_enabled = param & 0x1;
            ctx.stencil_test_func = (param >> 4) & 0x7;
            ctx.stencil_write_mask = (param >> 8) & 0xFF;
            ctx.stencil_ref = (param >> 16) & 0xFF;
            ctx.stencil_input_mask = param >> 24;
            break;
        case 0x106:
            ctx.stencil_fail_func = param & 0x7;
            ctx.stencil_depth_fail_func = (param >> 4) & 0x7;
            ctx.stencil_depth_pass_func = (param >> 8) & 0x7;
            break;
        case 0x107:
            ctx.depth_test_enabled = param & 0x1;
            ctx.depth_test_func = (param >> 4) & 0x7;
            for (int i = 0; i < 4; i++)
                ctx.rgba_write_enabled[i] = (param >> (i + 8)) & 0x1;
            ctx.depth_write_enabled = (param >> 12) & 0x1;
            break;
        case 0x115:
            ctx.allow_stencil_depth_write = param & 0x3;
            break;
        case 0x116:
            ctx.depth_format = param & 0x3;
            break;
        case 0x117:
            ctx.color_format = (param >> 16) & 0x7;
            break;
        case 0x11C:
            ctx.depth_buffer_base = (param & 0x0FFFFFFF) << 3;
            break;
        case 0x11D:
            ctx.color_buffer_base = (param & 0x0FFFFFFF) << 3;
            break;
        case 0x11E:
            ctx.frame_width = param & 0x7FF;
            ctx.frame_height = ((param >> 12) & 0x3FF) + 1;
            break;
        case 0x200:
            ctx.vtx_buffer_base = (param & 0x1FFFFFFE) << 3;
            break;
        case 0x201:
            ctx.attr_buffer_format_low = param;
            break;
        case 0x202:
            ctx.attr_buffer_format_hi = param & 0xFFFF;
            ctx.fixed_attr_mask = (param >> 16) & 0xFFF;
            ctx.total_vtx_attrs = (param >> 28) + 1;
            break;
        case 0x227:
            ctx.index_buffer_offs = (param & 0x0FFFFFFF);
            ctx.index_buffer_short = (param >> 31) != 0;
            break;
        case 0x228:
            ctx.vertices = param;
            break;
        case 0x22A:
            ctx.vtx_offset = param;
            break;
        case 0x22E:
            if (param != 0)
                draw_vtx_array(false);
            break;
        case 0x22F:
            if (param != 0)
                draw_vtx_array(true);
            break;
        case 0x232:
            ctx.fixed_attr_index = param & 0xF;
            ctx.fixed_attr_count = 0;
            ctx.vsh_input_counter = 0;
            break;
        case 0x242:
            ctx.vsh_inputs = (param & 0xF) + 1;
            break;
        case 0x244:
            ctx.gsh_enabled = param & 0x1;
            break;
        case 0x24A:
            ctx.vsh_outputs = (param & 0xF) + 1;
            break;
        case 0x252:
            ctx.gsh_mode = param & 0x3;
            ctx.gsh_fixed_vtx_num = ((param >> 8) & 0xF) + 1;
            ctx.gsh_stride = ((param >> 12) & 0xF) + 1;
            ctx.gsh_start_index = (param >> 16) & 0xF;
            break;
        case 0x25E:
            ctx.prim_mode = (param >> 8) & 0x3;
            break;
        case 0x25F:
            ctx.submitted_vertices = 0;
            ctx.gsh_attrs_input = 0;
            ctx.even_strip = true;
            break;
        case 0x280:
            ctx.gsh.bool_uniform = param & 0xFFFF;
            break;
        case 0x281:
        case 0x282:
        case 0x283:
        case 0x284:
            ctx.gsh.int_regs[reg - 0x281][0] = param & 0xFF;
            ctx.gsh.int_regs[reg - 0x281][1] = (param >> 8) & 0xFF;
            ctx.gsh.int_regs[reg - 0x281][2] = (param >> 16) & 0xFF;
            ctx.gsh.int_regs[reg - 0x281][3] = param >> 24;
            break;
        case 0x289:
            ctx.gsh.total_inputs = (param & 0xF) + 1;
            break;
        case 0x28A:
            ctx.gsh.entry_point = param & 0xFFFF;
            break;
        case 0x28B:
            for (int i = 0; i < 8; i++)
                ctx.gsh.input_mapping[i] = (param >> (i * 4)) & 0xF;
            break;
        case 0x28C:
            for (int i = 0; i < 8; i++)
                ctx.gsh.input_mapping[i + 8] = (param >> (i * 4)) & 0xF;
            break;
        case 0x290:
            ctx.gsh.float_uniform_index = param & 0xFF;
            ctx.gsh.float_uniform_mode = param >> 31;
            ctx.gsh.float_uniform_counter = 0;
            break;
        case 0x29B:
            ctx.gsh.code_index = (param & 0xFFF);
            break;
        case 0x2A5:
            ctx.gsh.op_desc_index = param & 0xFFF;
            break;
        case 0x2B0:
            ctx.vsh.bool_uniform = param & 0xFFFF;
            break;
        case 0x2B1:
        case 0x2B2:
        case 0x2B3:
        case 0x2B4:
            ctx.vsh.int_regs[reg - 0x2B1][0] = param & 0xFF;
            ctx.vsh.int_regs[reg - 0x2B1][1] = (param >> 8) & 0xFF;
            ctx.vsh.int_regs[reg - 0x2B1][2] = (param >> 16) & 0xFF;
            ctx.vsh.int_regs[reg - 0x2B1][3] = param >> 24;
            break;
        case 0x2B9:
            ctx.vsh.total_inputs = (param & 0xF) + 1;
            break;
        case 0x2BA:
            ctx.vsh.entry_point = param & 0xFFFF;
            break;
        case 0x2BB:
            for (int i = 0; i < 8; i++)
                ctx.vsh.input_mapping[i] = (param >> (i * 4)) & 0xF;
            break;
        case 0x2BC:
            for (int i = 0; i < 8; i++)
                ctx.vsh.input_mapping[i + 8] = (param >> (i * 4)) & 0xF;
            break;
        case 0x2C0:
            ctx.vsh.float_uniform_index = param & 0xFF;
            ctx.vsh.float_uniform_mode = param >> 31;
            ctx.vsh.float_uniform_counter = 0;
            break;
        case 0x2CB:
            ctx.vsh.code_index = param & 0xFFF;
            break;
        case 0x2D5:
            ctx.vsh.op_desc_index = param & 0xFFF;
            break;
        default:
            printf("[GPU] Unrecognized command $%04X ($%08X)\n", reg, param);
            break;
    }
}

void GPU::input_float_uniform(ShaderUnit &sh, uint32_t param)
{
    sh.float_uniform_buffer[sh.float_uniform_counter] = param;
    sh.float_uniform_counter++;

    if ((sh.float_uniform_mode && sh.float_uniform_counter == 4)
            || (!sh.float_uniform_mode && sh.float_uniform_counter == 3))
    {
        int index = sh.float_uniform_index;
        if (sh.float_uniform_mode)
        {
            //Float32
            for (int i = 0; i < 4; i++)
                sh.float_uniform[index][3 - i] = float24::FromFloat32(*(float*)&sh.float_uniform_buffer[i]);
        }
        else
        {
            //Float24
            sh.float_uniform[index].w = float24::FromRaw(sh.float_uniform_buffer[0] >> 8);
            sh.float_uniform[index].z = float24::FromRaw((
                                                       (sh.float_uniform_buffer[0] & 0xFF) << 16) |
                                                       ((sh.float_uniform_buffer[1] >> 16) & 0xFFFF));
            sh.float_uniform[index].y = float24::FromRaw((
                                                       (sh.float_uniform_buffer[1] & 0xFFFF) << 8) |
                                                       ((sh.float_uniform_buffer[2] >> 24) & 0xFF));
            sh.float_uniform[index].x = float24::FromRaw(sh.float_uniform_buffer[2] & 0xFFFFFF);
        }

        sh.float_uniform_index++;
        sh.float_uniform_counter = 0;
    }
}

void GPU::draw_vtx_array(bool is_indexed)
{
    printf("[GPU] DRAW_VTX_ARRAY (indexed: %d)\n", is_indexed);
    uint32_t index_base = ctx.vtx_buffer_base + ctx.index_buffer_offs;
    uint32_t index_offs = 0;

    uint64_t vtx_fmts = ctx.attr_buffer_format_low;
    vtx_fmts |= (uint64_t)ctx.attr_buffer_format_hi << 32ULL;
    for (unsigned int i = 0; i < ctx.vertices; i++)
    {
        uint16_t index;
        if (is_indexed)
        {
            if (ctx.index_buffer_short)
            {
                index = e->arm11_read16(0, index_base + index_offs);
                index_offs += 2;
            }
            else
            {
                index = e->arm11_read8(0, index_base + index_offs);
                index_offs++;
            }
        }
        else
            index = i + ctx.vtx_offset;

        //Initialize variable input attributes
        int attr = 0;
        int buffer = 0;
        while (attr < ctx.total_vtx_attrs)
        {
            if (!(ctx.fixed_attr_mask & (1 << attr)))
            {
                uint64_t cfg = ctx.attr_buffer_cfg1[buffer];
                cfg |= (uint64_t)ctx.attr_buffer_cfg2[buffer] << 32ULL;

                uint32_t addr = ctx.vtx_buffer_base + ctx.attr_buffer_offs[buffer];
                addr += ctx.attr_buffer_vtx_size[buffer] * index;
                for (unsigned int k = 0; k < ctx.attr_buffer_components[buffer]; k++)
                {
                    uint8_t vtx_format = (cfg >> (k * 4)) & 0xF;

                    if (vtx_format < 12)
                    {
                        vtx_format = (vtx_fmts >> (vtx_format * 4)) & 0xF;

                        uint8_t fmt = vtx_format & 0x3;
                        uint8_t size = ((vtx_format >> 2) & 0x3) + 1;

                        switch (fmt)
                        {
                            case 0:
                                //Signed byte
                                for (int comp = 0; comp < size; comp++)
                                {
                                    int8_t value = (int8_t)e->arm11_read8(0, addr + comp);
                                    ctx.vsh.input_attrs[attr][comp] = float24::FromFloat32(value);
                                }
                                addr += size;
                                break;
                            case 1:
                                //Unsigned byte
                                for (int comp = 0; comp < size; comp++)
                                {
                                    uint8_t value = e->arm11_read8(0, addr + comp);
                                    ctx.vsh.input_attrs[attr][comp] = float24::FromFloat32(value);
                                }
                                addr += size;
                                break;
                            case 2:
                                //Signed short
                                for (int comp = 0; comp < size; comp++)
                                {
                                    int16_t value = (int16_t)e->arm11_read16(0, addr + (comp * 2));
                                    ctx.vsh.input_attrs[attr][comp] = float24::FromFloat32(value);
                                }
                                addr += size * 2;
                                break;
                            case 3:
                                //Float
                                for (int comp = 0; comp < size; comp++)
                                {
                                    uint32_t temp = e->arm11_read32(0, addr + (comp * 4));
                                    float value;
                                    memcpy(&value, &temp, sizeof(float));
                                    ctx.vsh.input_attrs[attr][comp] = float24::FromFloat32(value);
                                }
                                addr += size * 4;
                                break;
                        }

                        //If some components are missing, initialize them with default values
                        //w = 1.0f, all others = 0.0f
                        for (int comp = size; comp < 4; comp++)
                        {
                            if (comp == 3)
                                ctx.vsh.input_attrs[attr][comp] = float24::FromFloat32(1.0f);
                            else
                                ctx.vsh.input_attrs[attr][comp] = float24::FromFloat32(0.0f);
                        }

                        attr++;
                    }
                    else
                    {
                        constexpr static int sizes[] = {4, 8, 12, 16};
                        addr += sizes[vtx_format - 12];
                    }
                }

                buffer++;
            }
            else
                attr++;
        }

        input_vsh_vtx();
    }
}

void GPU::input_vsh_vtx()
{
    exec_shader(ctx.vsh);

    if (ctx.gsh_enabled)
    {
        if (ctx.prim_mode != 3)
            EmuException::die("[GPU] Primitive mode not set to geometry when gsh is active!");
        //Place attributes in intermediate geometry buffer depending on mode
        //Execute a geometry shader when enough attributes are present
        switch (ctx.gsh_mode)
        {
            case 0:
                //Point mode - geometry shader takes GSH_INPUT_NUM / VSH_OUTPUT_NUM vertices as input
                for (int i = 0; i < ctx.vsh_outputs; i++)
                    ctx.gsh.input_attrs[ctx.gsh_attrs_input + i] = ctx.vsh.output_regs[i];
                ctx.gsh_attrs_input += ctx.vsh_outputs;

                if (ctx.gsh_attrs_input == ctx.gsh.total_inputs)
                {
                    exec_shader(ctx.gsh);
                    ctx.gsh_attrs_input = 0;

                    //Bool uniform register 15 is set after a geoshader executes for the first time.
                    ctx.gsh.bool_uniform |= 1 << 15;
                }
                break;
            default:
                EmuException::die("[GPU] Unrecognized gsh mode %d", ctx.gsh_mode);
        }
    }
    else
    {
        Vertex v;
        map_sh_output_to_vtx(ctx.vsh, v);

        submit_vtx(v, false);
    }
}

void GPU::map_sh_output_to_vtx(ShaderUnit &sh, Vertex &v)
{
    for (int i = 0; i < ctx.sh_output_total; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            uint8_t mapping = ctx.sh_output_mapping[i][j];
            switch (mapping)
            {
                case 0x00:
                case 0x01:
                case 0x02:
                case 0x03:
                    v.pos[mapping] = sh.output_regs[i][j];
                    break;
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                    v.quat[mapping - 0x04] = sh.output_regs[i][j];
                    break;
                case 0x08:
                case 0x09:
                case 0x0A:
                case 0x0B:
                    v.color[mapping - 0x08] = sh.output_regs[i][j];
                    break;
                case 0x0C:
                case 0x0D:
                    v.texcoords[0][mapping - 0x0C] = sh.output_regs[i][j];
                    break;
                case 0x0E:
                case 0x0F:
                    v.texcoords[1][mapping - 0x0E] = sh.output_regs[i][j];
                    break;
                case 0x10:
                    v.texcoords[0][3] = sh.output_regs[i][j];
                    break;
                case 0x12:
                case 0x13:
                case 0x14:
                    v.view[mapping - 0x12] = sh.output_regs[i][j];
                    break;
                case 0x16:
                case 0x17:
                    v.texcoords[2][mapping - 0x16] = sh.output_regs[i][j];
                    break;
            }
        }
    }
}

void GPU::submit_vtx(Vertex& v, bool winding)
{
    //Add vertex to the queue
    ctx.vertex_queue[ctx.submitted_vertices] = v;
    ctx.submitted_vertices++;

    if (ctx.submitted_vertices == 3)
    {
        //Copy queue into temporary variables to prevent rasterize_tri from overwriting them
        Vertex v0 = ctx.vertex_queue[0];
        Vertex v1 = ctx.vertex_queue[1];
        Vertex v2 = ctx.vertex_queue[2];

        if (ctx.prim_mode == 3 && winding)
        {
            process_tri(v1, v0, v2);
            ctx.gsh_winding = false;
        }
        else
            process_tri(v0, v1, v2);
        switch (ctx.prim_mode)
        {
            case 0:
            case 3:
                //Independent triangles/geometry shader primitives
                ctx.submitted_vertices = 0;
                break;
            case 1:
                //Triangle strip
                ctx.submitted_vertices--;

                //Preserve the original orientation of the strip
                if (ctx.even_strip)
                    ctx.vertex_queue[0] = ctx.vertex_queue[2];
                else
                    ctx.vertex_queue[1] = ctx.vertex_queue[2];

                ctx.even_strip = !ctx.even_strip;
                break;
            case 2:
                //Triangle fan
                ctx.vertex_queue[1] = ctx.vertex_queue[2];
                ctx.submitted_vertices--;
                break;
            default:
                EmuException::die("[GPU] Unrecognized primitive mode %d", ctx.prim_mode);
        }
    }
}

void GPU::process_tri(Vertex &v0, Vertex &v1, Vertex &v2)
{
    //There are 6 clipping planes. As a clipping operation can result in an extra vertex being produced,
    //the maximum amount of vertices a primitive can have is 9.
    std::vector<Vertex> output_list;
    std::vector<Vertex> input_list;

    output_list.push_back(v0);
    output_list.push_back(v1);
    output_list.push_back(v2);

    float24 one = float24::FromFloat32(1.0f);
    float24 zero = float24::FromFloat32(0.0f);
    float24 epsilon = float24::FromFloat32(0.000001f);

    Vec4<float24> clipping_planes[7] =
    {
        {-one, zero, zero, one},
        {one, zero, zero, one},
        {zero, -one, zero, one},
        {zero, one, zero, one},
        {zero, zero, -one, zero},
        {zero, zero, one, one},
        {zero, zero, zero, one} //epsilon is applied here
    };

    Vec4<float24> bias[7] =
    {
        {zero, zero, zero, zero},
        {zero, zero, zero, zero},
        {zero, zero, zero, zero},
        {zero, zero, zero, zero},
        {zero, zero, zero, zero},
        {zero, zero, zero, zero},
        {zero, zero, zero, epsilon}
    };

    for (int plane = 0; plane < 7; plane++)
    {
        std::swap(input_list, output_list);
        output_list.clear();

        Vertex ref = input_list.back();

        for (unsigned int i = 0; i < input_list.size(); i++)
        {
            Vertex vtx = input_list[i];
            float24 vtx_dp = dp4(vtx.pos + bias[plane], clipping_planes[plane]);
            float24 ref_dp = dp4(ref.pos + bias[plane], clipping_planes[plane]);

            bool vtx_inside = vtx_dp >= zero;
            bool ref_inside = ref_dp >= zero;

            float24 lerp_factor = ref_dp / (ref_dp - vtx_dp);
            Vertex intersection = vtx * lerp_factor + ref * (one - lerp_factor);

            if (vtx_inside)
            {
                if (!ref_inside)
                    output_list.push_back(intersection);

                output_list.push_back(vtx);
            }
            else if (ref_inside)
                output_list.push_back(intersection);

            ref = vtx;
        }

        //Check if we have enough vertices for a complete triangle
        if (output_list.size() < 3)
        {
            //printf("Clipped...\n");
            return;
        }
    }

    viewport_transform(output_list[0]);
    viewport_transform(output_list[1]);

    for (unsigned int i = 0; i < output_list.size() - 2; i++)
    {
        viewport_transform(output_list[2 + i]);

        v0 = output_list[0];
        v1 = output_list[1 + i];
        v2 = output_list[2 + i];

        rasterize_tri(v0, v1, v2);
    }
}

void GPU::viewport_transform(Vertex &v)
{
    //Multiply all vertex values (except w) by 1/w
    float24 inv_w = float24::FromFloat32(1.0f) / v.pos[3];
    v.pos *= inv_w;
    v.color *= inv_w;
    v.texcoords[0] *= inv_w;
    v.texcoords[1] *= inv_w;
    v.texcoords[2] *= inv_w;
    v.quat *= inv_w;
    v.view *= inv_w;

    v.pos[3] = inv_w;

    //Apply viewport transform
    //The y coordinate must also be inverted
    v.pos[0] += float24::FromFloat32(1.0f);
    v.pos[0] = (v.pos[0] * ctx.viewport_width) + float24::FromFloat32(ctx.viewport_x);

    v.pos[1] += float24::FromFloat32(1.0f);
    v.pos[1] = float24::FromFloat32(2.0f) - v.pos[1];
    v.pos[1] = (v.pos[1] * ctx.viewport_height) + float24::FromFloat32(ctx.viewport_y);

    //Round x and y to nearest whole number
    v.pos[0] = float24::FromFloat32(roundf(v.pos[0].ToFloat32() * 16.0));
    v.pos[1] = float24::FromFloat32(roundf(v.pos[1].ToFloat32() * 16.0));
}

// this is garbage, possible to go way faster.
static void order3(float a, float b, float c, uint8_t* order)
{
    if(a > b) {
        if(b > c) {
            order[0] = 2;
            order[1] = 1;
            order[2] = 0;
        } else {
            // a > b, b < c
            order[0] = 1;
            if(a > c) {
                order[2] = 0;
                order[1] = 2;
            } else {
                // c > a
                order[2] = 2;
                order[1] = 0;
            }
        }
    } else {
        // b > a
        if(a > c) {
            order[0] = 2;
            order[1] = 0;
            order[2] = 1;
        } else {
            // b > a, c > a
            order[0] = 0; // a
            if(b > c) {
                order[1] = 2;
                order[2] = 1;
            } else {
                order[1] = 1;
                order[2] = 2;
            }
        }
    }
}

// Returns positive value if the points are in counter-clockwise order
// 0 if they it's on the same line
// Negative value if they are in a clockwise order
// Basicly the cross product of (v2 - v1) and (v3 - v1) vectors
float24 orient2D(const Vertex &v1, const Vertex &v2, const Vertex &v3)
{
    return (v2.pos[0] - v1.pos[0]) * (v3.pos[1] - v1.pos[1]) - (v3.pos[0] - v1.pos[0]) * (v2.pos[1] - v1.pos[1]);
}

bool GPU::get_fill_rule_bias(Vertex &vtx, Vertex &line1, Vertex &line2)
{
    //This function prevents pixels on the right-side or flat bottom side of a triangle from being drawn.
    if (line1.pos[1] == line2.pos[1])
        return vtx.pos[1] < line1.pos[1];
    return vtx.pos[0] < line1.pos[0] + (line2.pos[0] - line1.pos[0]) *
                                                       (vtx.pos[1] - line1.pos[1]) /
                                                       (line2.pos[1] - line1.pos[1]);
}

void GPU::rasterize_tri(Vertex &v0, Vertex &v1, Vertex &v2)
{
    //The triangle rasterization code uses an approach with barycentric coordinates
    //Clear explanation can be read below:
    //https://fgiesen.wordpress.com/2013/02/06/the-barycentric-conspirac/

    //Check if texture combiners are unused - this lets us save time by not looping through all six of them
    ctx.texcomb_start = 0;
    for (int i = 0; i < 6; i++)
    {
        if (ctx.texcomb_rgb_source[i][0] == 0xF && ctx.texcomb_alpha_source[i][0] == 0xF &&
            ctx.texcomb_rgb_op[i] == 0 && ctx.texcomb_alpha_op[i] == 0 &&
            ctx.texcomb_rgb_operand[i][0] == 0 && ctx.texcomb_alpha_operand[i][0] == 0)
            continue;

        ctx.texcomb_start = i;
        break;
    }

    ctx.texcomb_end = 6;
    for (int i = 5; i >= 0; i--)
    {
        if (ctx.texcomb_rgb_source[i][0] == 0xF && ctx.texcomb_alpha_source[i][0] == 0xF &&
            ctx.texcomb_rgb_op[i] == 0 && ctx.texcomb_alpha_op[i] == 0 &&
            ctx.texcomb_rgb_operand[i][0] == 0 && ctx.texcomb_alpha_operand[i][0] == 0)
            continue;

        ctx.texcomb_end = i + 1;
        break;
    }

    std::swap(v1, v2);

    //Keep front face mode - reverse vertex order
    if (orient2D(v0, v1, v2).ToFloat32() < 0.0f)
    {
        if (ctx.cull_mode)
        {
            if (ctx.cull_mode == 1)
            {
                std::swap(v1, v2);
                if (orient2D(v0, v1, v2).ToFloat32() < 0.0f)
                    return;
            }
            else
                return;
        }
        else
            std::swap(v1, v2);
    }

    //Calculate bounding box of triangle
    int32_t min_x = std::min({v0.pos[0].ToFloat32(), v1.pos[0].ToFloat32(), v2.pos[0].ToFloat32()});
    int32_t min_y = std::min({v0.pos[1].ToFloat32(), v1.pos[1].ToFloat32(), v2.pos[1].ToFloat32()});
    int32_t max_x = std::max({v0.pos[0].ToFloat32(), v1.pos[0].ToFloat32(), v2.pos[0].ToFloat32()});
    int32_t max_y = std::max({v0.pos[1].ToFloat32(), v1.pos[1].ToFloat32(), v2.pos[1].ToFloat32()});

    min_x &= ~0xF;
    min_y &= ~0xF;
    min_x += 8;
    min_y += 8;
    max_x = (max_x + 0xF) & ~0xF;
    max_y = (max_y + 0xF) & ~0xF;

    Vertex min_corner;
    min_corner.pos[0] = float24::FromFloat32(min_x);
    min_corner.pos[1] = float24::FromFloat32(min_y);

    bool can_do_stencil = ctx.stencil_test_enabled && ctx.depth_format == 0x3;

    /*int32_t w1_row = orient2D(v1, v2, min_corner).ToFloat32();
    int32_t w2_row = orient2D(v2, v0, min_corner).ToFloat32();
    int32_t w3_row = orient2D(v0, v1, min_corner).ToFloat32();

    int32_t w1_dx = (v1.pos[1] - v2.pos[1]).ToFloat32() * 0x10;
    int32_t w2_dx = (v2.pos[1] - v0.pos[1]).ToFloat32() * 0x10;
    int32_t w3_dx = (v0.pos[1] - v1.pos[1]).ToFloat32() * 0x10;

    int32_t w1_dy = (v2.pos[0] - v1.pos[0]).ToFloat32() * 0x10;
    int32_t w2_dy = (v0.pos[0] - v2.pos[0]).ToFloat32() * 0x10;
    int32_t w3_dy = (v1.pos[0] - v0.pos[0]).ToFloat32() * 0x10;*/

    int bias0 = get_fill_rule_bias(v0, v1, v2) ? -1 : 0;
    int bias1 = get_fill_rule_bias(v1, v2, v0) ? -1 : 0;
    int bias2 = get_fill_rule_bias(v2, v0, v1) ? -1 : 0;

    //TODO: Parallelize this
    for (int32_t y = min_y; y < max_y; y += 0x10)
    {
        /*int32_t w1 = w1_row;
        int32_t w2 = w2_row;
        int32_t w3 = w3_row;*/
        for (int32_t x = min_x; x < max_x; x += 0x10)
        {
            Vertex temp;
            temp.pos[0] = float24::FromFloat32(x);
            temp.pos[1] = float24::FromFloat32(y);
            int32_t w1 = roundf(orient2D(v1, v2, temp).ToFloat32()) + bias0;
            int32_t w2 = roundf(orient2D(v2, v0, temp).ToFloat32()) + bias1;
            int32_t w3 = roundf(orient2D(v0, v1, temp).ToFloat32()) + bias2;
            //Is inside triangle?
            if ((w1 | w2 | w3) >= 0)
            {
                float24 f1 = float24::FromFloat32(w1);
                float24 f2 = float24::FromFloat32(w2);
                float24 f3 = float24::FromFloat32(w3);
                Vertex vtx;

                float24 divider = float24::FromFloat32(1.0f) / (v0.pos[3] * f1 +
                        v1.pos[3] * f2 + v2.pos[3] * f3);

                float24 z = (v0.pos[2] * f1 + v1.pos[2] * f2 + v2.pos[2] * f3) / (f1 + f2 + f3);

                float depth = (z * ctx.depth_scale + ctx.depth_offset).ToFloat32();

                if (!ctx.use_z_for_depth)
                    depth *= ((f1 + f2 + f3) * divider).ToFloat32();

                if (depth < 0.0)
                    depth = 0.0;
                if (depth > 1.0)
                    depth = 1.0;

                for (int i = 0; i < 4; i++)
                    vtx.color[i] = (v0.color[i] * f1 + v1.color[i] * f2 + v2.color[i] * f3) * divider;

                for (int i = 0; i < 2; i++)
                {
                    vtx.texcoords[0][i] = (v0.texcoords[0][i] * f1 + v1.texcoords[0][i] * f2 + v2.texcoords[0][i] * f3) * divider;
                    vtx.texcoords[1][i] = (v0.texcoords[1][i] * f1 + v1.texcoords[1][i] * f2 + v2.texcoords[1][i] * f3) * divider;
                    vtx.texcoords[2][i] = (v0.texcoords[2][i] * f1 + v1.texcoords[2][i] * f2 + v2.texcoords[2][i] * f3) * divider;
                }

                RGBA_Color source_color, frame_color;

                source_color.r = (uint8_t)roundf((vtx.color[0].ToFloat32() * 255.0));
                source_color.g = (uint8_t)roundf((vtx.color[1].ToFloat32() * 255.0));
                source_color.b = (uint8_t)roundf((vtx.color[2].ToFloat32() * 255.0));
                source_color.a = (uint8_t)roundf((vtx.color[3].ToFloat32() * 255.0));

                combine_textures(source_color, vtx);

                if (ctx.alpha_test_enabled)
                {
                    bool alpha_pass = true;
                    switch (ctx.alpha_test_func)
                    {
                        case 0:
                            //NEVER
                            alpha_pass = false;
                            break;
                        case 1:
                            //ALWAYS
                            break;
                        case 2:
                            //EQUAL
                            alpha_pass = source_color.a == ctx.alpha_test_ref;
                            break;
                        case 3:
                            //NOT EQUAL
                            alpha_pass = source_color.a != ctx.alpha_test_ref;
                            break;
                        case 4:
                            //LESS THAN
                            alpha_pass = source_color.a < ctx.alpha_test_ref;
                            break;
                        case 5:
                            //LESS THAN OR EQUAL
                            alpha_pass = source_color.a <= ctx.alpha_test_ref;
                            break;
                        case 6:
                            //GREATER THAN
                            alpha_pass = source_color.a > ctx.alpha_test_ref;
                            break;
                        case 7:
                            //GREATER THAN OR EQUAL
                            alpha_pass = source_color.a >= ctx.alpha_test_ref;
                            break;
                    }

                    if (!alpha_pass)
                        continue;
                }

                uint8_t stencil = 0;

                //The stencil test only works on 24-bit depth + 8-bit stencil
                if (can_do_stencil)
                {
                    uint32_t depth_addr = get_swizzled_tile_addr(ctx.depth_buffer_base,
                                                                 ctx.frame_width, x >> 4, y >> 4, 4);

                    stencil = e->arm11_read32(0, depth_addr) >> 24;
                    uint8_t dest = stencil & ctx.stencil_input_mask;
                    uint8_t ref = ctx.stencil_ref & ctx.stencil_input_mask;

                    bool pass = false;
                    switch (ctx.stencil_test_func)
                    {
                        case 0:
                            //NEVER
                            break;
                        case 1:
                            //ALWAYS
                            pass = true;
                            break;
                        case 2:
                            //EQUAL
                            pass = ref == dest;
                            break;
                        case 3:
                            //NEQUAL
                            pass = ref != dest;
                            break;
                        case 4:
                            //LESS THAN
                            pass = ref < dest;
                            break;
                        case 5:
                            //LESS THAN OR EQUAL
                            pass = ref <= dest;
                            break;
                        case 6:
                            //GREATER THAN
                            pass = ref > dest;
                            break;
                        case 7:
                            //GREATER THAN OR EQUAL
                            pass = ref >= dest;
                            break;
                    }

                    if (!pass)
                    {
                        update_stencil(depth_addr, stencil, ctx.stencil_ref, ctx.stencil_fail_func);
                        continue;
                    }
                }

                uint32_t depth_addr = 0;
                uint32_t old_depth = 0;
                uint32_t new_depth = 0;

                switch (ctx.depth_format)
                {
                    case 0x0:
                        depth_addr = get_swizzled_tile_addr(ctx.depth_buffer_base,
                                                             ctx.frame_width, x >> 4, y >> 4, 2);
                        new_depth = (uint32_t)(depth * 0xFFFF);
                        break;
                    case 0x2:
                        depth_addr = get_swizzled_tile_addr(ctx.depth_buffer_base,
                                                             ctx.frame_width, x >> 4, y >> 4, 3);
                        new_depth = (uint32_t)(depth * 0xFFFFFF);
                        break;
                    case 0x3:
                        depth_addr = get_swizzled_tile_addr(ctx.depth_buffer_base,
                                                             ctx.frame_width, x >> 4, y >> 4, 4);
                        new_depth = (uint32_t)(depth * 0xFFFFFF);
                        break;
                    default:
                        EmuException::die("[GPU] Unrecognized depth format %d\n", ctx.depth_format);
                }

                bool depth_passed = true;

                if (ctx.depth_test_enabled)
                {
                    switch (ctx.depth_format)
                    {
                        case 0:
                            old_depth = e->arm11_read16(0, depth_addr);
                            break;
                        case 2:
                        case 3:
                            old_depth = e->arm11_read16(0, depth_addr);
                            old_depth |= e->arm11_read8(0, depth_addr + 2) << 16;
                            break;
                    }
                    switch (ctx.depth_test_func)
                    {
                        case 0x0:
                            //NEVER
                            depth_passed = false;
                            break;
                        case 0x1:
                            //ALWAYS
                            break;
                        case 0x2:
                            //EQUAL
                            depth_passed = new_depth == old_depth;
                            break;
                        case 0x3:
                            //NEQUAL
                            depth_passed = new_depth != old_depth;
                            break;
                        case 0x4:
                            //LESS THAN
                            depth_passed = new_depth < old_depth;
                            break;
                        case 0x5:
                            //LESS THAN OR EQUAL
                            depth_passed = new_depth <= old_depth;
                            break;
                        case 0x6:
                            //GREATER THAN
                            depth_passed = new_depth > old_depth;
                            break;
                        case 0x7:
                            //GREATER THAN OR EQUAL
                            depth_passed = new_depth >= old_depth;
                            break;
                    }
                }

                if (!depth_passed)
                {
                    if (can_do_stencil)
                        update_stencil(depth_addr, stencil, ctx.stencil_ref, ctx.stencil_depth_fail_func);
                    continue;
                }

                //Note that writes to the depth buffer happen even if the depth test is disabled
                if (ctx.depth_write_enabled && (ctx.allow_stencil_depth_write & 0x2))
                {
                    switch (ctx.depth_format)
                    {
                        case 0x0:
                            e->arm11_write16(0, depth_addr, new_depth);
                            break;
                        case 0x2:
                        case 0x3:
                            e->arm11_write8(0, depth_addr, new_depth & 0xFF);
                            e->arm11_write8(0, depth_addr + 1, (new_depth >> 8) & 0xFF);
                            e->arm11_write8(0, depth_addr + 2, (new_depth >> 16) & 0xFF);
                            break;
                    }
                }

                if (can_do_stencil)
                    update_stencil(depth_addr, stencil, ctx.stencil_ref, ctx.stencil_depth_pass_func);

                uint32_t frame_addr = 0, frame = 0;
                switch (ctx.color_format)
                {
                    case 0:
                        frame_addr = get_swizzled_tile_addr(ctx.color_buffer_base, ctx.frame_width, x >> 4, y >> 4, 4);
                        frame = bswp32(e->arm11_read32(0, frame_addr));
                        break;
                    case 1:
                        frame_addr = get_swizzled_tile_addr(ctx.color_buffer_base, ctx.frame_width, x >> 4, y >> 4, 3);
                        frame = e->arm11_read8(0, frame_addr + 2);
                        frame |= e->arm11_read8(0, frame_addr + 1) << 8;
                        frame |= e->arm11_read8(0, frame_addr) << 16;
                        frame |= 0xFF << 24;
                        break;
                    case 2:
                    {
                        frame_addr = get_swizzled_tile_addr(ctx.color_buffer_base, ctx.frame_width, x >> 4, y >> 4, 2);
                        uint16_t temp = e->arm11_read16(0, frame_addr);
                        frame = Convert5To8(temp >> 11);
                        frame |= Convert5To8((temp >> 6) & 0x1F) << 8;
                        frame |= Convert5To8((temp >> 1) & 0x1F) << 16;
                        frame |= Convert1To8(temp & 0x1) << 24;
                    }
                        break;
                    case 3:
                    {
                        frame_addr = get_swizzled_tile_addr(ctx.color_buffer_base, ctx.frame_width, x >> 4, y >> 4, 2);
                        uint16_t temp = e->arm11_read16(0, frame_addr);
                        frame = Convert5To8(temp >> 11);
                        frame |= Convert6To8((temp >> 5) & 0x3F) << 8;
                        frame |= Convert5To8(temp & 0x1F) << 16;
                        frame |= 0xFF << 24;
                    }
                        break;
                    case 4:
                    {
                        frame_addr = get_swizzled_tile_addr(ctx.color_buffer_base, ctx.frame_width, x >> 4, y >> 4, 2);
                        uint16_t temp = e->arm11_read16(0, frame_addr);
                        frame = Convert4To8(temp >> 12);
                        frame |= Convert4To8((temp >> 8) & 0xF) << 8;
                        frame |= Convert4To8((temp >> 4) & 0xF) << 16;
                        frame |= Convert4To8(temp & 0xF) << 24;
                    }
                        break;
                    default:
                        EmuException::die("[GPU] Unrecognized color format $%02X\n", ctx.color_format);
                }

                frame_color.r = frame & 0xFF;
                frame_color.g = (frame >> 8) & 0xFF;
                frame_color.b = (frame >> 16) & 0xFF;
                frame_color.a = frame >> 24;

                blend_fragment(source_color, frame_color);

                if (!ctx.rgba_write_enabled[0])
                    source_color.r = frame & 0xFF;
                if (!ctx.rgba_write_enabled[1])
                    source_color.g = (frame >> 8) & 0xFF;
                if (!ctx.rgba_write_enabled[2])
                    source_color.b = (frame >> 16) & 0xFF;
                if (!ctx.rgba_write_enabled[3])
                    source_color.a = frame >> 24;

                uint32_t final_color = 0;

                switch (ctx.color_format)
                {
                    case 0:
                        final_color = source_color.r | (source_color.g << 8) | (source_color.b << 16) | (source_color.a << 24);
                        e->arm11_write32(0, frame_addr, bswp32(final_color));
                        break;
                    case 1:
                        e->arm11_write8(0, frame_addr + 2, source_color.r);
                        e->arm11_write8(0, frame_addr + 1, source_color.g);
                        e->arm11_write8(0, frame_addr, source_color.b);
                        break;
                    case 2:
                        final_color = Convert8To5(source_color.r) << 11;
                        final_color |= Convert8To5(source_color.g) << 6;
                        final_color |= Convert8To5(source_color.b) << 1;
                        final_color |= Convert8To1(source_color.a);
                        e->arm11_write16(0, frame_addr, final_color);
                        break;
                    case 3:
                        final_color = Convert8To5(source_color.r) << 11;
                        final_color |= Convert8To6(source_color.g) << 5;
                        final_color |= Convert8To5(source_color.b);
                        e->arm11_write16(0, frame_addr, final_color);
                        break;
                    case 4:
                        final_color = Convert8To4(source_color.r) << 12;
                        final_color |= Convert8To4(source_color.g) << 8;
                        final_color |= Convert8To4(source_color.b) << 4;
                        final_color |= Convert8To4(source_color.a);
                        e->arm11_write16(0, frame_addr, final_color);
                        break;
                }
            }
            /*w1 += w1_dx;
            w2 += w2_dx;
            w3 += w3_dx;*/
        }
        /*w1_row += w1_dy;
        w2_row += w2_dy;
        w3_row += w3_dy;*/
    }
}

void GPU::tex_lookup(int index, int coord_index, RGBA_Color &tex_color, Vertex &vtx)
{
    tex_color.r = 0;
    tex_color.g = 0;
    tex_color.b = 0;
    tex_color.a = 0;

    //TODO: A NULL/disabled texture should return the most recently rendered fragment color
    if (!ctx.tex_enable[index] || !ctx.tex_addr[index])
        return;

    int height = ctx.tex_height[index];
    int width = ctx.tex_width[index];
    int u = vtx.texcoords[coord_index][0].ToFloat32() * ctx.tex_width[index];
    int v = vtx.texcoords[coord_index][1].ToFloat32() * ctx.tex_height[index];

    switch (ctx.tex_swrap[index])
    {
        case 0:
            u = std::max(0, u);
            u = std::min(width - 1, u);
            break;
        case 1:
            if (u < 0 || u >= width)
            {
                tex_color = ctx.tex_border[index];
                return;
            }
            break;
        case 2:
            u = (unsigned int)u % width;
            break;
        case 3:
        {
            unsigned int temp = (unsigned int)u;
            temp %= (width << 1);
            if (temp >= (unsigned int)width)
                temp = (width << 1) - 1 - temp;
            u = (int)temp;
        }
            break;
        default:
            EmuException::die("[GPU] Unrecognized swrap %d", ctx.tex_swrap[index]);
    }

    switch (ctx.tex_twrap[index])
    {
        case 0:
            v = std::max(0, v);
            v = std::min(height - 1, v);
            break;
        case 1:
            if (v < 0 || v >= width)
            {
                tex_color = ctx.tex_border[index];
                return;
            }
            break;
        case 2:
            v = (unsigned int)v % height;
            break;
        case 3:
        {
            unsigned int temp = (unsigned int)v;
            temp %= (height << 1);
            if (temp >= (unsigned int)height)
                temp = (height << 1) - 1 - temp;
            v = (int)temp;
        }
            break;
        default:
            EmuException::die("[GPU] Unrecognized twrap %d", ctx.tex_twrap[index]);
    }

    //Texcoords are vertically flipped
    v = ctx.tex_height[index] - 1 - v;

    uint32_t addr = ctx.tex_addr[index];
    uint32_t texel;

    switch (ctx.tex_type[index])
    {
        case 0x0:
            //RGBA8888
            addr = get_swizzled_tile_addr(addr, width, u, v, 4);
            texel = bswp32(e->arm11_read32(0, addr));

            tex_color.r = texel & 0xFF;
            tex_color.g = (texel >> 8) & 0xFF;
            tex_color.b = (texel >> 16) & 0xFF;
            tex_color.a = texel >> 24;
            break;
        case 0x1:
            //RGB888
            addr = get_swizzled_tile_addr(addr, width, u, v, 3);
            tex_color.r = e->arm11_read8(0, addr + 2);
            tex_color.g = e->arm11_read8(0, addr + 1);
            tex_color.b = e->arm11_read8(0, addr);
            tex_color.a = 0xFF;
            break;
        case 0x2:
            //RGB5A1
            addr = get_swizzled_tile_addr(addr, width, u, v, 2);
            texel = e->arm11_read16(0, addr);

            //TODO: When alpha is disabled, it should be 0xFF
            tex_color.r = Convert5To8((texel >> 11) & 0x1F);
            tex_color.g = Convert5To8((texel >> 6) & 0x1F);
            tex_color.b = Convert5To8((texel >> 1) & 0x1F);
            tex_color.a = Convert1To8(texel & 0x1);
            break;
        case 0x3:
            //RGB565
            addr = get_swizzled_tile_addr(addr, width, u, v, 2);
            texel = e->arm11_read16(0, addr);

            tex_color.r = Convert5To8((texel >> 11) & 0x1F);
            tex_color.g = Convert6To8((texel >> 5) & 0x3F);
            tex_color.b = Convert5To8(texel & 0x1F);
            tex_color.a = 0xFF;
            break;
        case 0x4:
            //RGBA4444
            addr = get_swizzled_tile_addr(addr, width, u, v, 2);
            texel = e->arm11_read16(0, addr);

            tex_color.r = Convert4To8((texel >> 12) & 0xF);
            tex_color.g = Convert4To8((texel >> 8) & 0xF);
            tex_color.b = Convert4To8((texel >> 4) & 0xF);
            tex_color.a = Convert4To8(texel & 0xF);
            break;
        case 0x5:
            //IA8
            addr = get_swizzled_tile_addr(addr, width, u, v, 2);
            texel = e->arm11_read16(0, addr);

            //TODO: When alpha is disabled, R should be intensity and G should be alpha
            tex_color.r = texel >> 8;
            tex_color.g = texel >> 8;
            tex_color.b = texel >> 8;
            tex_color.a = texel & 0xFF;
            break;
        case 0x6:
            //RG8/HILO
            addr = get_swizzled_tile_addr(addr, width, u, v, 2);
            texel = e->arm11_read16(0, addr);

            tex_color.r = texel >> 8;
            tex_color.g = texel & 0xFF;
            tex_color.b = 0;
            tex_color.a = 0xFF;
            break;
        case 0x7:
            //I8
            addr = get_swizzled_tile_addr(addr, width, u, v, 1);
            texel = e->arm11_read8(0, addr);

            tex_color.r = texel;
            tex_color.g = texel;
            tex_color.b = texel;
            tex_color.a = 0xFF;
            break;
        case 0x8:
            //A8
            addr = get_swizzled_tile_addr(addr, width, u, v, 1);
            texel = e->arm11_read8(0, addr);

            tex_color.a = texel;
            break;
        case 0x9:
            //IA4
        {
            addr = get_swizzled_tile_addr(addr, width, u, v, 1);
            texel = e->arm11_read8(0, addr);
            uint8_t i = Convert4To8(texel >> 4);
            uint8_t a = Convert4To8(texel & 0xF);

            //TODO: When alpha is disabled, R should be intensity and G should be alpha
            tex_color.r = i;
            tex_color.g = i;
            tex_color.b = i;
            tex_color.a = a;
        }
            break;
        case 0xA:
            //I4
            addr = get_4bit_swizzled_addr(addr, width, u, v);
            if (u & 0x1)
                texel = e->arm11_read8(0, addr) >> 4;
            else
                texel = e->arm11_read8(0, addr) & 0xF;

            tex_color.r = Convert4To8(texel);
            tex_color.g = Convert4To8(texel);
            tex_color.b = Convert4To8(texel);
            tex_color.a = 0xFF;
            break;
        case 0xB:
            //A4
            addr = get_4bit_swizzled_addr(addr, width, u, v);
            if (u & 0x1)
                texel = e->arm11_read8(0, addr) >> 4;
            else
                texel = e->arm11_read8(0, addr) & 0xF;

            tex_color.a = Convert4To8(texel);
            break;
        case 0xC:
        case 0xD:
            //ETC1/ETC1A4
        {
            bool has_alpha = ctx.tex_type[index] & 0x1;

            //Get the tile we're on
            uint32_t offs = ((u & ~0x7) * 8) + ((v & ~0x7) * width);
            if (!has_alpha)
                offs >>= 1;
            addr += offs;

            //ETC1/ETC1A4 is split into four 4x4 subtiles rather than being swizzled.
            int tile_index = ((u & 0x7) / 4) + (2 * ((v & 0x7) / 4));
            u &= 0x3;
            v &= 0x3;

            addr += tile_index * (8 << has_alpha);

            if (has_alpha)
            {
                uint64_t alpha_data = e->arm11_read32(0, addr);
                alpha_data |= (uint64_t)e->arm11_read32(0, addr + 4) << 32ULL;

                addr += 8;
                tex_color.a = (alpha_data >> (4 * (u * 4 + v))) & 0xF;
                tex_color.a <<= 4;
            }
            else
                tex_color.a = 0xFF;

            uint64_t data = e->arm11_read32(0, addr);
            data |= (uint64_t)e->arm11_read32(0, addr + 4) << 32ULL;
            decode_etc1(tex_color, u, v, data);
        }
            break;
        default:
            EmuException::die("[GPU] Unrecognized tex format $%02X\n", ctx.tex_type[index]);
    }
}

void GPU::decode_etc1(RGBA_Color &tex_color, int u, int v, uint64_t data)
{
    constexpr static uint8_t modifier_table[8][2] =
    {
        {2, 8},
        {5, 17},
        {9, 29},
        {13, 42},
        {18, 60},
        {24, 80},
        {33, 106},
        {47, 183},
    };

    int texel = (u * 4) + v;
    uint16_t subindexes = data & 0xFFFF;
    uint16_t negation_flags = (data >> 16) & 0xFFFF;
    bool flip = (data >> 32) & 0x1;
    bool diff_mode = (data >> 33) & 0x1;
    uint8_t index2 = (data >> 34) & 0x7;
    uint8_t index1 = (data >> 37) & 0x7;

    if (flip)
        std::swap(u, v);

    if (diff_mode)
    {
        tex_color.r = (data >> 59) & 0x1F;
        tex_color.g = (data >> 51) & 0x1F;
        tex_color.b = (data >> 43) & 0x1F;

        if (u >= 2)
        {
            tex_color.r += SignExtend<3>((data >> 56) & 0x7);
            tex_color.g += SignExtend<3>((data >> 48) & 0x7);
            tex_color.b += SignExtend<3>((data >> 40) & 0x7);
        }

        tex_color.r <<= 3;
        tex_color.g <<= 3;
        tex_color.b <<= 3;
    }
    else
    {
        if (u < 2)
        {
            tex_color.r = (data >> 60) & 0xF;
            tex_color.g = (data >> 52) & 0xF;
            tex_color.b = (data >> 44) & 0xF;
        }
        else
        {
            tex_color.r = (data >> 56) & 0xF;
            tex_color.g = (data >> 48) & 0xF;
            tex_color.b = (data >> 40) & 0xF;
        }

        tex_color.r <<= 4;
        tex_color.g <<= 4;
        tex_color.b <<= 4;
    }

    int index;
    if (u < 2)
        index = index1;
    else
        index = index2;

    int modifier = modifier_table[index][(subindexes >> texel) & 0x1];

    if ((negation_flags >> texel) & 0x1)
        modifier = -modifier;

    tex_color.r += modifier;
    tex_color.g += modifier;
    tex_color.b += modifier;

    tex_color.r = std::min(255, tex_color.r);
    tex_color.r = std::max(0, tex_color.r);

    tex_color.g = std::min(255, tex_color.g);
    tex_color.g = std::max(0, tex_color.g);

    tex_color.b = std::min(255, tex_color.b);
    tex_color.b = std::max(0, tex_color.b);
}

void GPU::get_tex0(RGBA_Color &tex_color, Vertex &vtx)
{
    switch (ctx.tex0_type)
    {
        case 0:
            tex_lookup(0, 0, tex_color, vtx);
            break;
        case 5:
            //Disabled texture
            //TODO: Should this return the previously rendered color?
            tex_color = {0, 0, 0, 0};
            break;
        default:
            EmuException::die("[GPU] Unrecognized tex0 type %d", ctx.tex0_type);
    }
}

void GPU::get_tex1(RGBA_Color &tex_color, Vertex &vtx)
{
    tex_lookup(1, 1, tex_color, vtx);
}

void GPU::get_tex2(RGBA_Color &tex_color, Vertex &vtx)
{
    if (ctx.tex2_uses_tex1_coords)
        tex_lookup(2, 1, tex_color, vtx);
    else
        tex_lookup(2, 2, tex_color, vtx);
}

void GPU::combine_textures(RGBA_Color &source, Vertex& vtx)
{
    RGBA_Color primary = source;
    RGBA_Color prev = source;
    RGBA_Color comb_buffer = {0, 0, 0, 0}, next_comb_buffer = ctx.texcomb_buffer;

    RGBA_Color tex[4];

    get_tex0(tex[0], vtx);
    get_tex1(tex[1], vtx);
    get_tex2(tex[2], vtx);

    for (int i = ctx.texcomb_start; i < ctx.texcomb_end; i++)
    {
        RGBA_Color sources[3], alpha_sources[3], operands[3];
        int32_t source_a;

        for (int j = 0; j < 3; j++)
        {
            switch (ctx.texcomb_rgb_source[i][j])
            {
                case 0x0:
                    sources[j] = primary;
                    break;
                case 0x1:
                    sources[j] = primary;
                    break;
                case 0x2:
                    sources[j] = primary;
                    break;
                case 0x3:
                    sources[j] = tex[0];
                    break;
                case 0x4:
                    sources[j] = tex[1];
                    break;
                case 0x5:
                    sources[j] = tex[2];
                    break;
                case 0xD:
                    sources[j] = comb_buffer;
                    break;
                case 0xE:
                    sources[j] = ctx.texcomb_const[i];
                    break;
                case 0xF:
                    sources[j] = prev;
                    break;
                default:
                    EmuException::die("[GPU] Unrecognized texcomb RGB source $%02X",
                                      ctx.texcomb_rgb_source[i][j]);
            }

            source_a = sources[j].a;

            switch (ctx.texcomb_alpha_source[i][j])
            {
                case 0x0:
                    alpha_sources[j] = primary;
                    break;
                case 0x1:
                    alpha_sources[j] = primary;
                    break;
                case 0x2:
                    alpha_sources[j] = primary;
                    break;
                case 0x3:
                    alpha_sources[j] = tex[0];
                    break;
                case 0x4:
                    alpha_sources[j] = tex[1];
                    break;
                case 0x5:
                    alpha_sources[j] = tex[2];
                    break;
                case 0xD:
                    alpha_sources[j] = comb_buffer;
                    break;
                case 0xE:
                    alpha_sources[j] = ctx.texcomb_const[i];
                    break;
                case 0xF:
                    alpha_sources[j] = prev;
                    break;
                default:
                    EmuException::die("[GPU] Unrecognized texcomb alpha source $%02X",
                                      ctx.texcomb_alpha_source[i][j]);
            }

            switch (ctx.texcomb_rgb_operand[i][j])
            {
                case 0x0:
                    operands[j] = sources[j];
                    break;
                case 0x1:
                    operands[j].r = 255 - sources[j].r;
                    operands[j].g = 255 - sources[j].g;
                    operands[j].b = 255 - sources[j].b;
                    break;
                case 0x2:
                    operands[j].r = source_a;
                    operands[j].g = source_a;
                    operands[j].b = source_a;
                    break;
                case 0x3:
                    operands[j].r = 255 - source_a;
                    operands[j].g = 255 - source_a;
                    operands[j].b = 255 - source_a;
                    break;
                case 0x4:
                    operands[j].r = sources[j].r;
                    operands[j].g = sources[j].r;
                    operands[j].b = sources[j].r;
                    break;
                case 0x5:
                    operands[j].r = 255 - sources[j].r;
                    operands[j].g = 255 - sources[j].r;
                    operands[j].b = 255 - sources[j].r;
                    break;
                case 0x8:
                    operands[j].r = sources[j].g;
                    operands[j].g = sources[j].g;
                    operands[j].b = sources[j].g;
                    break;
                case 0x9:
                    operands[j].r = 255 - sources[j].g;
                    operands[j].g = 255 - sources[j].g;
                    operands[j].b = 255 - sources[j].g;
                    break;
                case 0xC:
                    operands[j].r = sources[j].b;
                    operands[j].g = sources[j].b;
                    operands[j].b = sources[j].b;
                    break;
                case 0xD:
                    operands[j].r = 255 - sources[j].b;
                    operands[j].g = 255 - sources[j].b;
                    operands[j].b = 255 - sources[j].b;
                    break;
                default:
                    EmuException::die("[GPU] Unrecognized texcomb RGB operand $%02X",
                                      ctx.texcomb_rgb_operand[i][j]);
            }

            switch (ctx.texcomb_alpha_operand[i][j])
            {
                case 0x0:
                    operands[j].a = alpha_sources[j].a;
                    break;
                case 0x1:
                    operands[j].a = 255 - alpha_sources[j].a;
                    break;
                case 0x2:
                    operands[j].a = alpha_sources[j].r;
                    break;
                case 0x3:
                    operands[j].a = 255 - alpha_sources[j].r;
                    break;
                case 0x4:
                    operands[j].a = alpha_sources[j].g;
                    break;
                case 0x5:
                    operands[j].a = 255 - alpha_sources[j].g;
                    break;
                case 0x6:
                    operands[j].a = alpha_sources[j].b;
                    break;
                case 0x7:
                    operands[j].a = 255 - alpha_sources[j].b;
                    break;
                default:
                    EmuException::die("[GPU] Unrecognized texcomb alpha operand $%02X",
                                      ctx.texcomb_alpha_operand[i][j]);
            }
        }

        switch (ctx.texcomb_rgb_op[i])
        {
            case 0:
                prev.r = operands[0].r;
                prev.g = operands[0].g;
                prev.b = operands[0].b;
                break;
            case 1:
                prev.r = (operands[0].r * operands[1].r) / 255;
                prev.g = (operands[0].g * operands[1].g) / 255;
                prev.b = (operands[0].b * operands[1].b) / 255;
                break;
            case 2:
                prev.r = std::min(255, operands[0].r + operands[1].r);
                prev.g = std::min(255, operands[0].g + operands[1].g);
                prev.b = std::min(255, operands[0].b + operands[1].b);
                break;
            case 3:
                prev.r = std::min(255, operands[0].r + operands[1].r - 128);
                prev.g = std::min(255, operands[0].g + operands[1].g - 128);
                prev.b = std::min(255, operands[0].b + operands[1].b - 128);

                prev.r = std::max(0, prev.r);
                prev.g = std::max(0, prev.g);
                prev.b = std::max(0, prev.b);
                break;
            case 4:
                prev.r = (operands[0].r * operands[2].r + operands[1].r * (255 - operands[2].r)) / 255;
                prev.g = (operands[0].g * operands[2].g + operands[1].g * (255 - operands[2].g)) / 255;
                prev.b = (operands[0].b * operands[2].b + operands[1].b * (255 - operands[2].b)) / 255;
                break;
            case 5:
                prev.r = std::max(0, operands[0].r - operands[1].r);
                prev.g = std::max(0, operands[0].g - operands[1].g);
                prev.b = std::max(0, operands[0].b - operands[1].b);
                break;
            case 8:
                prev.r = ((operands[0].r * operands[1].r) + (255 * operands[2].r)) / 255;
                prev.g = ((operands[0].g * operands[1].g) + (255 * operands[2].g)) / 255;
                prev.b = ((operands[0].b * operands[1].b) + (255 * operands[2].b)) / 255;

                prev.r = std::min(255, prev.r);
                prev.g = std::min(255, prev.g);
                prev.b = std::min(255, prev.b);
                break;
            case 9:
                prev.r = (std::min(255, (operands[0].r + operands[1].r)) * operands[2].r) / 255;
                prev.g = (std::min(255, (operands[0].g + operands[1].g)) * operands[2].g) / 255;
                prev.b = (std::min(255, (operands[0].b + operands[1].b)) * operands[2].b) / 255;
                break;
            default:
                EmuException::die("[GPU] Unrecognized texcomb RGB op $%02X", ctx.texcomb_rgb_op[i]);
        }

        switch (ctx.texcomb_alpha_op[i])
        {
            case 0:
                prev.a = operands[0].a;
                break;
            case 1:
                prev.a = (operands[0].a * operands[1].a) / 255;
                break;
            case 2:
                prev.a = std::min(255, operands[0].a + operands[1].a);
                break;
            case 3:
                prev.a = std::min(255, operands[0].a + operands[1].a - 128);
                prev.a = std::max(0, prev.a);
                break;
            case 4:
                prev.a = (operands[0].a * operands[2].a + operands[1].a * (255 - operands[2].a)) / 255;
                break;
            case 5:
                prev.a = std::max(0, operands[0].a - operands[1].a);
                break;
            case 8:
                prev.a = ((operands[0].a * operands[1].a) + (255 * operands[2].a)) / 255;
                prev.a = std::min(255, prev.a);
                break;
            case 9:
                prev.a = (std::min(255, (operands[0].a + operands[1].a)) * operands[2].a) / 255;
                break;
            default:
                EmuException::die("[GPU] Unrecognized texcomb alpha op $%02X", ctx.texcomb_alpha_op[i]);
        }

        switch (ctx.texcomb_rgb_scale[i])
        {
            case 1:
                prev.r = std::min(255, prev.r << 1);
                prev.g = std::min(255, prev.g << 1);
                prev.b = std::min(255, prev.b << 1);
                break;
            case 2:
                prev.r = std::min(255, prev.r << 2);
                prev.g = std::min(255, prev.g << 2);
                prev.b = std::min(255, prev.b << 2);
                break;
        }

        switch (ctx.texcomb_alpha_scale[i])
        {
            case 1:
                prev.a = std::min(255, prev.a << 1);
                break;
            case 2:
                prev.a = std::min(255, prev.a << 2);
                break;
        }

        comb_buffer = next_comb_buffer;

        if (ctx.texcomb_rgb_buffer_update[i])
        {
            next_comb_buffer.r = prev.r;
            next_comb_buffer.g = prev.g;
            next_comb_buffer.b = prev.b;
        }

        if (ctx.texcomb_alpha_buffer_update[i])
            next_comb_buffer.a = prev.a;
    }

    source = prev;
}

void GPU::blend_fragment(RGBA_Color &source, RGBA_Color &frame)
{
    switch (ctx.fragment_op)
    {
        case 0:
            //Regular blending
            if (ctx.blend_mode == 0)
                do_logic_op(source, frame);
            else
                do_alpha_blending(source, frame);
            break;
        default:
            EmuException::die("[GPU] Unrecognized fragment operation %d", ctx.fragment_op);
    }
}

void GPU::do_alpha_blending(RGBA_Color &source, RGBA_Color &frame)
{
    RGBA_Color temp_source = source;
    RGBA_Color dest = frame;
    switch (ctx.blend_rgb_src_func)
    {
        case 0x0:
            //Zero
            source.r = 0;
            source.g = 0;
            source.b = 0;
            break;
        case 0x1:
            //One
            source.r *= 255;
            source.g *= 255;
            source.b *= 255;
            break;
        case 0x2:
            //Source color
            source.r *= temp_source.r;
            source.g *= temp_source.g;
            source.b *= temp_source.b;
            break;
        case 0x4:
            //Dest color
            source.r *= dest.r;
            source.g *= dest.g;
            source.b *= dest.b;
            break;
        case 0x6:
            //Source alpha
            source.r *= temp_source.a;
            source.g *= temp_source.a;
            source.b *= temp_source.a;
            break;
        case 0x9:
            //One minus dest alpha
            source.r *= 255 - dest.a;
            source.g *= 255 - dest.a;
            source.b *= 255 - dest.a;
            break;
        default:
            EmuException::die("[GPU] Unrecognized blend RGB src function $%02X",
                              ctx.blend_rgb_src_func);
    }

    switch (ctx.blend_alpha_src_func)
    {
        case 0x0:
            source.a = 0;
            break;
        case 0x1:
            source.a *= 255;
            break;
        case 0x2:
            source.a *= temp_source.a;
            break;
        case 0x4:
            source.a *= dest.a;
            break;
        case 0x6:
            source.a *= temp_source.a;
            break;
        case 0x9:
            source.a *= 255 - dest.a;
            break;
        default:
            EmuException::die("[GPU] Unrecognized blend alpha src function $%02X",
                              ctx.blend_alpha_src_func);
    }

    switch (ctx.blend_rgb_dst_func)
    {
        case 0x0:
            frame.r = 0;
            frame.g = 0;
            frame.b = 0;
            break;
        case 0x1:
            frame.r *= 255;
            frame.g *= 255;
            frame.b *= 255;
            break;
        case 0x2:
            frame.r *= temp_source.r;
            frame.g *= temp_source.g;
            frame.b *= temp_source.b;
            break;
        case 0x6:
            frame.r *= temp_source.a;
            frame.g *= temp_source.a;
            frame.b *= temp_source.a;
            break;
        case 0x7:
            frame.r *= 255 - temp_source.a;
            frame.g *= 255 - temp_source.a;
            frame.b *= 255 - temp_source.a;
            break;
        case 0x8:
            frame.r *= dest.a;
            frame.g *= dest.a;
            frame.b *= dest.a;
            break;
        case 0xB:
            frame.r *= 255 - ctx.blend_color.r;
            frame.g *= 255 - ctx.blend_color.g;
            frame.b *= 255 - ctx.blend_color.b;
            break;
        case 0xC:
            frame.r *= ctx.blend_color.a;
            frame.g *= ctx.blend_color.a;
            frame.b *= ctx.blend_color.a;
            break;
        default:
            EmuException::die("[GPU] Unrecognized blend rgb dest function $%02X",
                              ctx.blend_rgb_dst_func);
    }

    switch (ctx.blend_alpha_dst_func)
    {
        case 0x0:
            frame.a = 0;
            break;
        case 0x1:
            frame.a *= 255;
            break;
        case 0x2:
            frame.a *= temp_source.a;
            break;
        case 0x6:
            frame.a *= temp_source.a;
            break;
        case 0x7:
            frame.a *= 255 - temp_source.a;
            break;
        case 0x8:
            frame.a *= dest.a;
            break;
        case 0xC:
            frame.a *= ctx.blend_color.a;
            break;
        default:
            EmuException::die("[GPU] Unrecognized blend alpha dest function $%02X",
                              ctx.blend_alpha_dst_func);
    }

    switch (ctx.blend_rgb_equation)
    {
        case 0:
        case 5:
        case 6:
        case 7:
            source.r = (source.r + frame.r) / 255;
            source.g = (source.g + frame.g) / 255;
            source.b = (source.b + frame.b) / 255;
            break;
        case 1:
            source.r = (source.r - frame.r) / 255;
            source.g = (source.g - frame.g) / 255;
            source.b = (source.b - frame.b) / 255;
            break;
        case 2:
            source.r = (frame.r - source.r) / 255;
            source.g = (frame.g - source.g) / 255;
            source.b = (frame.b - source.b) / 255;
            break;
        default:
            EmuException::die("[GPU] Unrecognized blend RGB equation %d",
                              ctx.blend_rgb_equation);
    }

    switch (ctx.blend_alpha_equation)
    {
        case 0:
        case 5:
        case 6:
        case 7:
            source.a = (source.a + frame.a) / 255;
            break;
        case 1:
            source.a = (source.a - frame.a) / 255;
            break;
        case 2:
            source.a = (frame.a - source.a) / 255;
            break;
        default:
            EmuException::die("[GPU] Unrecognized blend alpha equation %d",
                              ctx.blend_alpha_equation);
    }

    //Clamp final color to 0-0xFF range
    source.r = std::min(0xFF, source.r);
    source.g = std::min(0xFF, source.g);
    source.b = std::min(0xFF, source.b);
    source.a = std::min(0xFF, source.a);

    source.r = std::max(0, source.r);
    source.g = std::max(0, source.g);
    source.b = std::max(0, source.b);
    source.a = std::max(0, source.a);
}

void GPU::do_logic_op(RGBA_Color &source, RGBA_Color &frame)
{
    switch (ctx.logic_op)
    {
        case 0:
            //Clear
            source = {0, 0, 0, 0};
            break;
        case 1:
            //AND
            source.r &= frame.r;
            source.g &= frame.g;
            source.b &= frame.b;
            source.a &= frame.a;
            break;
        case 2:
            //AND Reverse
            source.r &= ~frame.r;
            source.g &= ~frame.g;
            source.b &= ~frame.b;
            source.a &= ~frame.a;
            break;
        case 3:
            //Copy - leave source as-is
            break;
        case 4:
            //Set
            source = {255, 255, 255, 255};
            break;
        case 5:
            //Inverted copy
            source.r = ~source.r;
            source.g = ~source.g;
            source.b = ~source.b;
            source.a = ~source.a;
            break;
        case 6:
            //Noop
            source = frame;
            break;
        case 7:
            //Invert
            source.r = ~frame.r;
            source.g = ~frame.g;
            source.b = ~frame.b;
            source.a = ~frame.a;
            break;
        case 8:
            //NAND
            source.r = ~(source.r & frame.r);
            source.g = ~(source.g & frame.g);
            source.b = ~(source.b & frame.b);
            source.a = ~(source.a & frame.a);
            break;
        case 9:
            //OR
            source.r |= frame.r;
            source.g |= frame.g;
            source.b |= frame.b;
            source.a |= frame.a;
            break;
        case 10:
            //NOR
            source.r = ~(source.r | frame.r);
            source.g = ~(source.g | frame.g);
            source.b = ~(source.b | frame.b);
            source.a = ~(source.a | frame.a);
            break;
        case 11:
            //XOR
            source.r ^= frame.r;
            source.g ^= frame.g;
            source.b ^= frame.b;
            source.a ^= frame.a;
            break;
        case 12:
            //XNOR
            source.r = ~(source.r ^ frame.r);
            source.g = ~(source.g ^ frame.g);
            source.b = ~(source.b ^ frame.b);
            source.a = ~(source.a ^ frame.a);
            break;
        case 13:
            //Inverted AND
            source.r = ~source.r & frame.r;
            source.g = ~source.g & frame.g;
            source.b = ~source.b & frame.b;
            source.a = ~source.a & frame.a;
            break;
        case 14:
            //OR Reverse
            source.r |= ~frame.r;
            source.g |= ~frame.g;
            source.b |= ~frame.b;
            source.a |= ~frame.a;
            break;
        case 15:
            //Inverted OR
            source.r = ~source.r | frame.r;
            source.g = ~source.g | frame.g;
            source.b = ~source.b | frame.b;
            source.a = ~source.a | frame.a;
            break;
        default:
            EmuException::die("[GPU] Unrecognized logic op $%02X", ctx.logic_op);
    }
}

void GPU::update_stencil(uint32_t addr, uint8_t old, uint8_t ref, uint8_t func)
{
    uint8_t new_stencil = 0;
    switch (func)
    {
        case 0:
            //Keep
            new_stencil = old;
            break;
        case 1:
            //Zero
            new_stencil = 0;
            break;
        case 2:
            //Replace
            new_stencil = ref;
            break;
        case 3:
            //Increment
            if (old == 255)
                new_stencil = 255;
            else
                new_stencil = old + 1;
            break;
        case 4:
            //Decrement
            if (old == 0)
                new_stencil = 0;
            else
                new_stencil = old - 1;
            break;
        case 5:
            //Invert
            new_stencil = ~old;
            break;
        case 6:
            //Increment with wrap
            new_stencil = old + 1;
            break;
        case 7:
            //Decrement with wrap
            new_stencil = old - 1;
            break;
    }

    if (ctx.allow_stencil_depth_write & 0x1)
        e->arm11_write8(0, addr + 3, (new_stencil & ctx.stencil_write_mask) | (old & ~ctx.stencil_write_mask));
}

void GPU::exec_shader(ShaderUnit& sh)
{
    //Prepare input registers
    for (int i = 0; i < sh.total_inputs; i++)
    {
        int mapping = sh.input_mapping[i];
        sh.input_regs[mapping] = sh.input_attrs[i];
    }

    /*for (int i = 0; i < sh.total_inputs; i++)
    {
        printf("v%d: ", i);
        for (int j = 0; j < 4; j++)
            printf("%f ", sh.input_regs[i][j].ToFloat32());
        printf("\n");
    }*/

    /*for (int i = 0; i < 96; i++)
    {
        printf("c%d: ", i);
        for (int j = 0; j < 4; j++)
            printf("%f ", sh.float_uniform[i][j].ToFloat32());
        printf("\n");
    }*/

    sh.loop_ptr = 0;
    sh.call_ptr = 0;
    sh.if_ptr = 0;
    sh.pc = sh.entry_point * 4;

    bool ended = false;
    while (!ended)
    {
        uint32_t instr = *(uint32_t*)&sh.code[sh.pc];
        //printf("[GPU] [$%04X] $%08X\n", sh.pc, instr);
        sh.pc += 4;

        switch (instr >> 26)
        {
            case 0x00:
                shader_add(sh, instr);
                break;
            case 0x01:
                shader_dp3(sh, instr);
                break;
            case 0x02:
                shader_dp4(sh, instr);
                break;
            case 0x03:
                shader_dph(sh, instr);
                break;
            case 0x08:
                shader_mul(sh, instr);
                break;
            case 0x0C:
                shader_max(sh, instr);
                break;
            case 0x0E:
                shader_rcp(sh, instr);
                break;
            case 0x0F:
                shader_rsq(sh, instr);
                break;
            case 0x12:
                shader_mova(sh, instr);
                break;
            case 0x13:
                shader_mov(sh, instr);
                break;
            case 0x1B:
                shader_slti(sh, instr);
                break;
            case 0x20:
                shader_break(sh, instr);
                break;
            case 0x21:
                //NOP
                break;
            case 0x22:
                ended = true;
                break;
            case 0x23:
                shader_breakc(sh, instr);
                break;
            case 0x24:
                shader_call(sh, instr);
                break;
            case 0x25:
                shader_callc(sh, instr);
                break;
            case 0x26:
                shader_callu(sh, instr);
                break;
            case 0x27:
                shader_ifu(sh, instr);
                break;
            case 0x28:
                shader_ifc(sh, instr);
                break;
            case 0x29:
                shader_loop(sh, instr);
                break;
            case 0x2A:
                shader_emit(sh, instr);
                break;
            case 0x2B:
                shader_setemit(sh, instr);
                break;
            case 0x2C:
                shader_jmpc(sh, instr);
                break;
            case 0x2D:
                shader_jmpu(sh, instr);
                break;
            case 0x2E:
            case 0x2F:
                shader_cmp(sh, instr);
                break;
            case 0x30:
            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x35:
            case 0x36:
            case 0x37:
                shader_madi(sh, instr);
                break;
            case 0x38:
            case 0x39:
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
                shader_mad(sh, instr);
                break;
            default:
                EmuException::die("[GPU] Unrecognized shader opcode $%02X (instr:$%08X pc:$%04X)",
                                  instr >> 26, instr, sh.pc - 4);
        }

        if (sh.loop_ptr > 0)
        {
            //Note: LOOP is inclusive, so this is actually checking if the PC is DST + 4
            if (sh.pc == sh.loop_cmp_stack[sh.loop_ptr - 1])
            {
                sh.pc = sh.loop_stack[sh.loop_ptr - 1];
                sh.loop_iter_stack[sh.loop_ptr - 1]--;
                sh.loop_ctr_reg += sh.loop_inc_reg[sh.loop_ptr - 1];

                if (!sh.loop_iter_stack[sh.loop_ptr - 1])
                    sh.loop_ptr--;
            }
        }

        if (sh.if_ptr > 0)
        {
            if (sh.pc == sh.if_cmp_stack[sh.if_ptr - 1])
            {
                sh.if_ptr--;
                sh.pc = sh.if_stack[sh.if_ptr];
            }
        }

        if (sh.call_ptr > 0)
        {
            if (sh.pc == sh.call_cmp_stack[sh.call_ptr - 1])
            {
                sh.call_ptr--;
                sh.pc = sh.call_stack[sh.call_ptr];
            }
        }
    }

    /*for (int i = 0; i < 7; i++)
    {
        printf("o%d: ", i);
        for (int j = 0; j < 4; j++)
            printf("%f ", sh.output_regs[i][j].ToFloat32());
        printf("\n");
    }*/
}

Vec4<float24> GPU::swizzle_sh_src(Vec4<float24> src, uint32_t op_desc, int src_type)
{
    bool negate;
    uint8_t compsel;

    Vec4<float24> result;

    switch (src_type)
    {
        case 1:
            negate = (op_desc >> 4) & 0x1;
            compsel = (op_desc >> 5) & 0xFF;
            break;
        case 2:
            negate = (op_desc >> 13) & 0x1;
            compsel = (op_desc >> 14) & 0xFF;
            break;
        case 3:
            negate = (op_desc >> 22) & 0x1;
            compsel = (op_desc >> 23) & 0xFF;
            break;
        default:
            EmuException::die("[GPU] Unrecognized src_type %d in swizzle_sh_src", src_type);
    }

    for (int i = 0; i < 4; i++)
    {
        int comp = (compsel >> (i * 2)) & 0x3;
        result[3 - i] = src[comp];

        if (negate)
            result[3 - i] = -result[3 - i];
    }

    return result;
}

Vec4<float24> GPU::get_src(ShaderUnit& sh, uint8_t src)
{
    if (src < 0x10)
        return sh.input_regs[src];
    if (src < 0x20)
        return sh.temp_regs[src - 0x10];
    return sh.float_uniform[src - 0x20];
}

int GPU::get_idx1(ShaderUnit& sh, uint8_t idx1, uint8_t src1)
{
    if (src1 < 0x20)
        return 0;

    switch (idx1)
    {
        case 0:
            return 0;
        case 1:
            return sh.addr_reg[0].ToFloat32();
        case 2:
            return sh.addr_reg[1].ToFloat32();
        case 3:
            return sh.loop_ctr_reg;
        default:
            EmuException::die("[GPU] Unrecognized idx %d", idx1);
            return 0;
    }
}

void GPU::set_sh_dest(ShaderUnit &sh, uint8_t dst, float24 value, int index)
{
    //printf("[GPU] Setting sh reg $%02X:%d to %f\n", dst, index, value.ToFloat32());
    if (dst < 0x7)
        sh.output_regs[dst][index] = value;
    else if (dst >= 0x10 && dst < 0x20)
        sh.temp_regs[dst - 0x10][index] = value;
    else
        EmuException::die("[GPU] Unrecognized dst $%02X in set_sh_dest", dst);
}

bool GPU::shader_meets_cond(ShaderUnit& sh, uint32_t instr)
{
    uint8_t cond = (instr >> 22) & 0x3;
    bool ref_y = (instr >> 24) & 0x1;
    bool ref_x = (instr >> 25) & 0x1;

    bool passed = false;

    switch (cond)
    {
        case 0:
            passed = sh.cmp_regs[0] == ref_x || sh.cmp_regs[1] == ref_y;
            break;
        case 1:
            passed = sh.cmp_regs[0] == ref_x && sh.cmp_regs[1] == ref_y;
            break;
        case 2:
            passed = sh.cmp_regs[0] == ref_x;
            break;
        case 3:
            passed = sh.cmp_regs[1] == ref_y;
            break;
    }

    return passed;
}

void GPU::shader_add(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
            set_sh_dest(sh, dest, src[0][3 - i] + src[1][3 - i], 3 - i);
    }
}

void GPU::shader_dp3(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    float24 dot_product = float24::FromFloat32(0.0f);

    for (int i = 0; i < 3; i++)
        dot_product += src[0][i] * src[1][i];

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
            set_sh_dest(sh, dest, dot_product, 3 - i);
    }
}

void GPU::shader_dp4(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    float24 dot_product = dp4(src[0], src[1]);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
            set_sh_dest(sh, dest, dot_product, 3 - i);
    }
}

void GPU::shader_dph(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    src[0][3] = float24::FromFloat32(1.0);

    float24 dot_product = dp4(src[0], src[1]);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
            set_sh_dest(sh, dest, dot_product, 3 - i);
    }
}

void GPU::shader_mul(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
            set_sh_dest(sh, dest, src[0][3 - i] * src[1][3 - i], 3 - i);
    }
}

void GPU::shader_max(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
        {
            float24 a = src[0][3 - i];
            float24 b = src[1][3 - i];
            float24 value = (a > b) ? a : b;
            set_sh_dest(sh, dest, value, 3 - i);
        }
    }
}

void GPU::shader_rcp(ShaderUnit& sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src = get_src(sh, src1);
    src = swizzle_sh_src(src, op_desc, 1);

    float24 value = src[0];
    value = float24::FromFloat32(1.0f) / value;

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
        {
            set_sh_dest(sh, dest, value, 3 - i);
        }
    }
}

void GPU::shader_rsq(ShaderUnit& sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src = get_src(sh, src1);
    src = swizzle_sh_src(src, op_desc, 1);

    float24 value = float24::FromFloat32(sqrtf(src[0].ToFloat32()));
    value = float24::FromFloat32(1.0f) / value;

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
        {
            set_sh_dest(sh, dest, value, 3 - i);
        }
    }
}

void GPU::shader_mova(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src = get_src(sh, src1);
    src = swizzle_sh_src(src, op_desc, 1);

    if (dest_mask & (1 << 3))
        sh.addr_reg[0] = src[0];

    if (dest_mask & (1 << 2))
        sh.addr_reg[1] = src[1];
}

void GPU::shader_mov(ShaderUnit& sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src = get_src(sh, src1);
    src = swizzle_sh_src(src, op_desc, 1);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
        {
            set_sh_dest(sh, dest, src[3 - i], 3 - i);
        }
    }
}

void GPU::shader_slti(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x7F;
    uint8_t src1 = (instr >> 14) & 0x1F;
    uint8_t idx2 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx2 = get_idx1(sh, idx2, src2);

    src2 += idx2;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
        {
            float24 value;
            if (src[0][3 - i].ToFloat32() < src[1][3 - i].ToFloat32())
                value = float24::FromFloat32(1.0);
            else
                value = float24::Zero();

            set_sh_dest(sh, dest, value, 3 - i);
        }
    }
}

void GPU::shader_break(ShaderUnit &sh, uint32_t instr)
{
    sh.pc = sh.loop_stack[sh.loop_ptr - 1];
    sh.loop_ptr--;
}

void GPU::shader_breakc(ShaderUnit &sh, uint32_t instr)
{
    uint8_t cond = (instr >> 22) & 0x3;
    bool ref_y = (instr >> 24) & 0x1;
    bool ref_x = (instr >> 25) & 0x1;

    bool passed;

    switch (cond)
    {
        case 0:
            passed = sh.cmp_regs[0] == ref_x || sh.cmp_regs[1] == ref_y;
            break;
        case 1:
            passed = sh.cmp_regs[0] == ref_x && sh.cmp_regs[1] == ref_y;
            break;
        case 2:
            passed = sh.cmp_regs[0] == ref_x;
            break;
        case 3:
            passed = sh.cmp_regs[1] == ref_y;
            break;
    }

    if (passed)
    {
        sh.pc = sh.loop_stack[sh.loop_ptr - 1];
        sh.loop_ptr--;
    }
}

void GPU::shader_call(ShaderUnit &sh, uint32_t instr)
{
    uint8_t num = instr & 0xFF;

    uint16_t dst = (instr >> 10) & 0xFFF;

    sh.call_stack[sh.call_ptr] = sh.pc;
    sh.call_cmp_stack[sh.call_ptr] = (dst + num) * 4;
    sh.call_ptr++;

    sh.pc = dst * 4;
}

void GPU::shader_callc(ShaderUnit &sh, uint32_t instr)
{
    uint8_t num = instr & 0xFF;

    uint16_t dst = (instr >> 10) & 0xFFF;

    if (shader_meets_cond(sh, instr))
    {
        sh.call_stack[sh.call_ptr] = sh.pc;
        sh.call_cmp_stack[sh.call_ptr] = (dst + num) * 4;
        sh.call_ptr++;

        sh.pc = dst * 4;
    }
}

void GPU::shader_callu(ShaderUnit &sh, uint32_t instr)
{
    uint8_t num = instr & 0xFF;

    uint16_t dst = (instr >> 10) & 0xFFF;

    uint8_t bool_id = (instr >> 22) & 0xF;

    if (sh.bool_uniform & (1 << bool_id))
    {
        sh.call_stack[sh.call_ptr] = sh.pc;
        sh.call_cmp_stack[sh.call_ptr] = (dst + num) * 4;
        sh.call_ptr++;

        sh.pc = dst * 4;
    }
}

void GPU::shader_ifu(ShaderUnit &sh, uint32_t instr)
{
    uint8_t num = instr & 0xFF;

    uint16_t dst = (instr >> 10) & 0xFFF;

    uint8_t bool_id = (instr >> 22) & 0xF;

    if (sh.bool_uniform & (1 << bool_id))
    {
        sh.if_cmp_stack[sh.if_ptr] = dst * 4;
        sh.if_stack[sh.if_ptr] = (dst + num) * 4;
        sh.if_ptr++;
    }
    else
        sh.pc = dst * 4;
}

void GPU::shader_ifc(ShaderUnit &sh, uint32_t instr)
{
    uint8_t num = instr & 0xFF;

    uint16_t dst = (instr >> 10) & 0xFFF;

    if (shader_meets_cond(sh, instr))
    {
        sh.if_cmp_stack[sh.if_ptr] = dst * 4;
        sh.if_stack[sh.if_ptr] = (dst + num) * 4;
        sh.if_ptr++;
    }
    else
        sh.pc = dst * 4;
}

void GPU::shader_loop(ShaderUnit &sh, uint32_t instr)
{
    uint16_t dst = (instr >> 10) & 0xFFF;

    uint8_t int_id = (instr >> 22) & 0xF;

    sh.loop_stack[sh.loop_ptr] = sh.pc;
    sh.loop_cmp_stack[sh.loop_ptr] = (dst * 4) + 4;
    sh.loop_iter_stack[sh.loop_ptr] = sh.int_regs[int_id][0] + 1;
    sh.loop_ctr_reg = sh.int_regs[int_id][1];
    sh.loop_inc_reg[sh.loop_ptr] = sh.int_regs[int_id][2];

    sh.loop_ptr++;
}

void GPU::shader_emit(ShaderUnit &sh, uint32_t instr)
{
    map_sh_output_to_vtx(sh, ctx.gsh_vtx_buffer[ctx.gsh_vtx_id]);

    if (ctx.gsh_emit_prim)
    {
        for (int i = 0; i < 3; i++)
            submit_vtx(ctx.gsh_vtx_buffer[i], ctx.gsh_winding);
    }
}

void GPU::shader_setemit(ShaderUnit &sh, uint32_t instr)
{
    ctx.gsh_winding = (instr >> 22) & 0x1;
    ctx.gsh_emit_prim = (instr >> 23) & 0x1;
    ctx.gsh_vtx_id = (instr >> 24) & 0x3;
}

void GPU::shader_jmpc(ShaderUnit &sh, uint32_t instr)
{
    uint16_t dst = (instr >> 10) & 0xFFF;

    if (shader_meets_cond(sh, instr))
        sh.pc = dst * 4;
}

void GPU::shader_jmpu(ShaderUnit &sh, uint32_t instr)
{
    bool cmp_bit = !(instr & 0x1);

    uint16_t dst = (instr >> 10) & 0xFFF;

    uint8_t bool_id = (instr >> 22) & 0xF;

    if (((sh.bool_uniform >> bool_id) & 0x1) == cmp_bit)
        sh.pc = dst * 4;
}

void GPU::shader_cmp(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;

    idx1 = get_idx1(sh, idx1, src1);

    src1 += idx1;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    uint8_t cmp_ops[2];
    cmp_ops[0] = (instr >> 24) & 0x7;
    cmp_ops[1] = (instr >> 21) & 0x7;

    for (int i = 0; i < 2; i++)
    {
        //printf("[GPU] Comparing %f to %f\n", src[0][i].ToFloat32(), src[1][i].ToFloat32());
        switch (cmp_ops[i])
        {
            case 0:
                sh.cmp_regs[i] = src[0][i] == src[1][i];
                break;
            case 1:
                sh.cmp_regs[i] = src[0][i] != src[1][i];
                break;
            case 2:
                sh.cmp_regs[i] = src[0][i] < src[1][i];
                break;
            case 3:
                sh.cmp_regs[i] = src[0][i] <= src[1][i];
                break;
            case 4:
                sh.cmp_regs[i] = src[0][i] > src[1][i];
                break;
            case 5:
                sh.cmp_regs[i] = src[0][i] >= src[1][i];
                break;
            default:
                EmuException::die("[GPU] Unrecognized sh CMP op $%02X", cmp_ops[i]);
        }

        //printf("[GPU] Result of cmp op: %d\n", sh.cmp_regs[i]);
    }
}

void GPU::shader_madi(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x1F];

    uint8_t src3 = (instr >> 5) & 0x7F;
    uint8_t src2 = (instr >> 12) & 0x1F;
    uint8_t src1 = (instr >> 17) & 0x1F;
    uint8_t idx3 = (instr >> 22) & 0x3;
    uint8_t dest = (instr >> 24) & 0x1F;

    idx3 = get_idx1(sh, idx3, src3);

    src3 += idx3;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[3];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);
    src[2] = swizzle_sh_src(get_src(sh, src3), op_desc, 3);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
        {
            float24 value = src[2][3 - i] + (src[1][3 - i] * src[0][3 - i]);
            set_sh_dest(sh, dest, value, 3 - i);
        }
    }
}

void GPU::shader_mad(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x1F];

    uint8_t src3 = (instr >> 5) & 0x1F;
    uint8_t src2 = (instr >> 10) & 0x7F;
    uint8_t src1 = (instr >> 17) & 0x1F;
    uint8_t idx2 = (instr >> 22) & 0x3;
    uint8_t dest = (instr >> 24) & 0x1F;

    idx2 = get_idx1(sh, idx2, src2);

    src2 += idx2;

    uint8_t dest_mask = op_desc & 0xF;

    Vec4<float24> src[3];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);
    src[2] = swizzle_sh_src(get_src(sh, src3), op_desc, 3);

    for (int i = 0; i < 4; i++)
    {
        if (dest_mask & (1 << i))
        {
            float24 value = src[2][3 - i] + (src[1][3 - i] * src[0][3 - i]);
            set_sh_dest(sh, dest, value, 3 - i);
        }
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
        case 0x0034:
            return (cmd_engine.busy << 31) | (memfill[0].busy << 27) | (memfill[1].busy << 26);
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
                    uint32_t cycles = memfill[index].end - memfill[index].start;
                    scheduler->add_event([this](uint64_t param) { this->do_memfill(param);}, cycles,
                            ARM11_CLOCKRATE, index);
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
        case 0x0C08:
            dma.disp_output_width = value & 0xFFFF;
            dma.disp_output_height = value >> 16;
            printf("[GPU] DMA output width/height: $%08X\n", value);
            break;
        case 0x0C0C:
            dma.disp_input_width = value & 0xFFFF;
            dma.disp_input_height = value >> 16;
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
                scheduler->add_event([this](uint64_t param) { this->do_transfer_engine_dma(param);},
                    1000, ARM11_CLOCKRATE);
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
                scheduler->add_event([this](uint64_t param) { this->do_command_engine_dma(param);},
                    cmd_engine.size, ARM11_CLOCKRATE);
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
        case 0x70:
            return fb->color_format;
        case 0x78:
            return fb->buffer_select;
        case 0x90:
            return fb->stride;
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
        case 0x90:
            printf("[GPU] Write fb%d stride: $%08X\n", index, value);
            fb->stride = value;
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

void GPU::set_lcd_init(bool init)
{
    lcd_initialized = init;
}

void GPU::set_screenfill(int index, uint32_t value)
{
    printf("[GPU] Set screenfill%d: $%08X\n", index, value);
    framebuffers[index].screenfill_color = value & 0xFFFFFF;
    framebuffers[index].screenfill_enabled = (value >> 24) & 0x1;
}
