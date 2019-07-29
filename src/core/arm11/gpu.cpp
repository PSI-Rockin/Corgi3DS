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
        start = screen_fb->left_addr_b;
    else
        start = screen_fb->left_addr_a;

    int index = x + (y * 240);
    uint32_t color = 0xFF000000;
    switch (screen_fb->color_format)
    {
        case 0:
            color = bswp32(e->arm11_read32(0, start + (index * 4)));
            break;
        case 1:
            color |= e->arm11_read8(0, start + (index * 3)) << 16;
            color |= e->arm11_read8(0, start + (index * 3) + 1) << 8;
            color |= e->arm11_read8(0, start + (index * 3) + 2);
            break;
        case 2:
        {
            uint16_t cin = e->arm11_read16(0, start + (index * 2));
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

        uint32_t output_addr = dma.output_addr;

        uint32_t input_delta = 1;
        uint32_t output_delta = 1;

        uint8_t input_format = (dma.flags >> 8) & 0x7;
        uint8_t output_format = (dma.flags >> 12) & 0x7;

        const static int format_sizes[] = {4, 3, 2, 2, 2, 0, 0};

        input_delta *= format_sizes[input_format];
        output_delta *= format_sizes[output_format];

        //if (dma.flags & (1 << 24))
            //input_delta <<= 1;

        if (dma.disp_input_width > dma.disp_output_width)
            input_delta *= dma.disp_input_width / dma.disp_output_width;
        else if (dma.disp_output_width > dma.disp_input_width)
            input_delta *= dma.disp_output_width / dma.disp_input_width;

        for (unsigned int y = 0; y < dma.disp_input_height; y++)
        {
            for (unsigned int x = 0; x < dma.disp_input_width; x++)
            {
                uint32_t color = 0;

                uint32_t input_addr = get_swizzled_tile_addr(
                            dma.input_addr, dma.disp_input_width, x, y, input_delta);

                switch (input_format)
                {
                    case 0:
                        color = bswp32(e->arm11_read32(0, input_addr));
                        break;
                    case 2:
                    {
                        color = e->arm11_read16(0, input_addr);
                        uint32_t pr = ((color >> 11) & 0x1F) << 3; //byte-aligned pixel red
                        uint32_t pb = ((color >>  0) & 0x1F) << 3; //byte-aligned pixel blue
                        uint32_t pg = ((color >>  5) & 0x3F) << 2; //byte-aligned pixel green

                        color = (pb << 16) | (pg << 8) | pr;
                        break;
                    }
                    case 4:
                    {
                        color = bswp16(e->arm11_read16(0, input_addr));

                        uint8_t a = color >> 12;
                        uint8_t b = (color >> 8) & 0xF;
                        uint8_t g = (color >> 4) & 0xF;
                        uint8_t r = color & 0xF;

                        color = (r << 4) | (g << 12) | (b << 20) | (a << 28);
                    }
                        break;
                    default:
                        EmuException::die("[GPU] Unrecognized input format %d\n", input_format);
                }

                uint8_t r = color & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = (color >> 16) & 0xFF;
                uint8_t a = color >> 24;

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
                        color |= b >> 3;
                        color |= (g >> 2) << 5;
                        color |= (r >> 3) << 11;
                        e->arm11_write16(0, output_addr, color);
                        break;
                    case 4:
                        color = 0;
                        color |= (r >> 4);
                        color |= (g >> 4) << 4;
                        color |= (b >> 4) << 8;
                        color |= (a >> 4) << 12;

                        e->arm11_write16(0, output_addr, bswp16(color));
                        break;
                    default:
                        EmuException::die("[GPU] Unrecognized output format %d\n", output_format);
                }

                output_addr += output_delta;

                if (dma.flags & (1 << 24))
                    x++;
            }
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
    if ((reg >= 0x0C0 && reg < 0x0E0) || (reg >= 0x0F0 && reg < 0x0FC))
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

                ctx.texcomb_alpha_operand[index][0] = (param >> 16) & 0xF;
                ctx.texcomb_alpha_operand[index][1] = (param >> 20) & 0xF;
                ctx.texcomb_alpha_operand[index][2] = (param >> 24) & 0xF;
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
                    submit_vertex();
                    ctx.vsh_input_counter = 0;
                }
            }
        }
        return;
    }

    //Vertex Shader float uniform upload
    if (reg >= 0x2C1 && reg < 0x2C9)
    {
        ctx.vsh.float_uniform_buffer[ctx.vsh.float_uniform_counter] = param;
        ctx.vsh.float_uniform_counter++;

        if ((ctx.vsh.float_uniform_mode && ctx.vsh.float_uniform_counter == 4)
                || (!ctx.vsh.float_uniform_mode && ctx.vsh.float_uniform_counter == 3))
        {
            int index = ctx.vsh.float_uniform_index;
            if (ctx.vsh.float_uniform_mode)
            {
                //Float32
                for (int i = 0; i < 4; i++)
                    ctx.vsh.float_uniform[index][3 - i] =
                            float24::FromFloat32(*(float*)&ctx.vsh.float_uniform_buffer[i]);
            }
            else
            {
                //Float24
                ctx.vsh.float_uniform[index].w = float24::FromRaw(ctx.vsh.float_uniform_buffer[0] >> 8);
                ctx.vsh.float_uniform[index].z = float24::FromRaw((
                                                           (ctx.vsh.float_uniform_buffer[0] & 0xFF) << 16) |
                                                           ((ctx.vsh.float_uniform_buffer[1] >> 16) & 0xFFFF));
                ctx.vsh.float_uniform[index].y = float24::FromRaw((
                                                           (ctx.vsh.float_uniform_buffer[1] & 0xFFFF) << 8) |
                                                           ((ctx.vsh.float_uniform_buffer[2] >> 24) & 0xFF));
                ctx.vsh.float_uniform[index].x = float24::FromRaw(ctx.vsh.float_uniform_buffer[2] & 0xFFFFFF);
            }

            ctx.vsh.float_uniform_index++;
            ctx.vsh.float_uniform_counter = 0;
        }
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
        case 0x04F:
            ctx.vsh_output_total = param & 0x7;
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

            ctx.vsh_output_mapping[index][0] = param & 0x1F;
            ctx.vsh_output_mapping[index][1] = (param >> 8) & 0x1F;
            ctx.vsh_output_mapping[index][2] = (param >> 16) & 0x1F;
            ctx.vsh_output_mapping[index][3] = (param >> 24) & 0x1F;
        }
            break;
        case 0x068:
            ctx.viewport_x = SignExtend<10>(param & 0x3FF);
            ctx.viewport_y = SignExtend<10>((param >> 16) & 0x3FF);
            break;
        case 0x080:
            ctx.tex_enable[0] = param & 0x1;
            ctx.tex_enable[1] = (param >> 1) & 0x1;
            ctx.tex_enable[2] = (param >> 2) & 0x1;
            ctx.tex3_coords = (param >> 8) & 0x3;
            ctx.tex_enable[3] = (param >> 10) & 0x3;
            ctx.tex2_uses_tex1_coords = (param >> 13) & 0x1;
            break;
        case 0x081:
            ctx.tex_border[0].r = param & 0xFF;
            ctx.tex_border[0].g = (param >> 8) & 0xFF;
            ctx.tex_border[0].b = (param >> 16) & 0xFF;
            ctx.tex_border[0].a = param >> 24;
            break;
        case 0x082:
            ctx.tex_height[0] = param & 0x3FF;
            ctx.tex_width[0] = (param >> 16) & 0x3FF;
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
            ctx.tex_height[((reg - 0x092) / 8) + 1] = param & 0x3FF;
            ctx.tex_width[((reg - 0x092) / 8) + 1] = (param >> 16) & 0x3FF;
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
        case 0x103:
            ctx.blend_color.r = param & 0xFF;
            ctx.blend_color.g = (param >> 8) & 0xFF;
            ctx.blend_color.b = (param >> 16) & 0xFF;
            ctx.blend_color.a = param >> 24;
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
        case 0x25E:
            ctx.prim_mode = (param >> 8) & 0x3;
            break;
        case 0x25F:
            ctx.submitted_vertices = 0;
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

void GPU::draw_vtx_array(bool is_indexed)
{
    printf("[GPU] DRAW_ARRAY_ELEMENTS\n");
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

        printf("Index: %d\n", index);

        //Initialize variable input attributes
        for (unsigned int j = 0; j < ctx.total_vtx_attrs; j++)
        {
            if (!(ctx.fixed_attr_mask & (1 << j)))
            {
                uint64_t cfg = ctx.attr_buffer_cfg1[j];
                cfg |= (uint64_t)ctx.attr_buffer_cfg2[j] << 32ULL;

                uint32_t addr = ctx.vtx_buffer_base + ctx.attr_buffer_offs[j];
                addr += ctx.attr_buffer_vtx_size[j] * index;
                for (unsigned int k = 0; k < ctx.attr_buffer_components[j]; k++)
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
                                    ctx.vsh.input_attrs[j][comp] = float24::FromFloat32(value);
                                }
                                addr += size;
                                break;
                            case 1:
                                //Unsigned byte
                                for (int comp = 0; comp < size; comp++)
                                {
                                    uint8_t value = e->arm11_read8(0, addr + comp);
                                    ctx.vsh.input_attrs[j][comp] = float24::FromFloat32(value);
                                }
                                addr += size;
                                break;
                            case 2:
                                //Signed short
                                for (int comp = 0; comp < size; comp++)
                                {
                                    int16_t value = (int16_t)e->arm11_read16(0, addr + (comp * 2));
                                    ctx.vsh.input_attrs[j][comp] = float24::FromFloat32(value);
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
                                    ctx.vsh.input_attrs[j][comp] = float24::FromFloat32(value);
                                }
                                addr += size * 4;
                                break;
                        }

                        //If some components are missing, initialize them with default values
                        //w = 1.0f, all others = 0.0f
                        for (int comp = size; comp < 4; comp++)
                        {
                            if (comp == 3)
                                ctx.vsh.input_attrs[j][comp] = float24::FromFloat32(1.0f);
                            else
                                ctx.vsh.input_attrs[j][comp] = float24::FromFloat32(0.0f);
                        }
                    }
                    else
                    {
                        constexpr static int sizes[] = {4, 8, 12, 16};
                        addr += sizes[vtx_format - 12];
                    }
                }
            }
        }

        submit_vertex();
    }
}

void GPU::submit_vertex()
{
    //Run the vertex shader
    exec_shader(ctx.vsh);

    //TODO: Run geometry shader?
    if (ctx.prim_mode == 3)
        return;

    //Map output registers to vertex variables
    Vertex v;
    for (int i = 0; i < ctx.vsh_output_total; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            uint8_t mapping = ctx.vsh_output_mapping[i][j];
            switch (mapping)
            {
                case 0x00:
                case 0x01:
                case 0x02:
                case 0x03:
                    v.pos[mapping] = ctx.vsh.output_regs[i][j];
                    break;
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                    v.quat[mapping - 0x04] = ctx.vsh.output_regs[i][j];
                    break;
                case 0x08:
                case 0x09:
                case 0x0A:
                case 0x0B:
                    v.color[mapping - 0x08] = ctx.vsh.output_regs[i][j];
                    break;
                case 0x0C:
                case 0x0D:
                    v.texcoords[0][mapping - 0x0C] = ctx.vsh.output_regs[i][j];
                    break;
                case 0x0E:
                case 0x0F:
                    v.texcoords[1][mapping - 0x0E] = ctx.vsh.output_regs[i][j];
                    break;
                case 0x10:
                    v.texcoords[0][3] = ctx.vsh.output_regs[i][j];
                    break;
                case 0x12:
                case 0x13:
                case 0x14:
                    v.view[mapping - 0x12] = ctx.vsh.output_regs[i][j];
                    break;
                case 0x16:
                case 0x17:
                    v.texcoords[2][mapping - 0x16] = ctx.vsh.output_regs[i][j];
                    break;
            }
        }
    }

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

    //Textures are laid from bottom to top, so we invert the T coordinate
    v.texcoords[0][1] = float24::FromFloat32(1.0f) - v.texcoords[0][1];

    //Apply viewport transform
    v.pos[0] += float24::FromFloat32(1.0f);
    v.pos[0] = (v.pos[0] * ctx.viewport_width) + float24::FromFloat32(ctx.viewport_x);

    v.pos[1] += float24::FromFloat32(1.0f);
    v.pos[1] = float24::FromFloat32(2.0f) - v.pos[1];
    v.pos[1] = (v.pos[1] * ctx.viewport_height) + float24::FromFloat32(ctx.viewport_y);

    //Round x and y to nearest whole number
    v.pos[0] = float24::FromFloat32(roundf(v.pos[0].ToFloat32()));
    v.pos[1] = float24::FromFloat32(roundf(v.pos[1].ToFloat32()));

    //Add vertex to the queue
    ctx.vertex_queue[ctx.submitted_vertices] = v;
    ctx.submitted_vertices++;

    if (ctx.submitted_vertices == 3)
    {
        rasterize_tri();
        switch (ctx.prim_mode)
        {
            case 0:
                //Independent triangles
                ctx.submitted_vertices = 0;
                break;
            case 1:
                //Triangle strip
                for (int i = 1; i < 3; i++)
                    ctx.vertex_queue[i - 1] = ctx.vertex_queue[i];
                ctx.submitted_vertices--;
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

void GPU::rasterize_tri() {
    // This is a "scanline" algorithm which reduces flops/pixel
    //  at the cost of a longer setup time.

    // per-pixel work:
    //         add   mult   div
    // stupid  13     7      1
    // old      5     3      1
    // scan     3     0      0
    //
    // per-line work:
    //         add   mult   div
    // stupid  0     0      0
    // old     3     0      0
    // scan    1     1      0   (possible to do 1 add, but rounding issues with floats occur)

    // it divides a triangle like this:

    //             * v0
    //
    //     v1  * ----
    //
    //
    //               * v2

    // where v0, v1, v2 are floating point pixel locations, ordered from low to high
    // (this triangles also has a positive area because the vertices are CCW)

    printf("[GPU] Rasterizing triangle\n");

    printf("Positions: ");
    for (int i = 0; i < 3; i++)
    {
        printf("(");
        for (int j = 0; j < 3; j++)
        {
            printf("%f", ctx.vertex_queue[i].pos[j].ToFloat32());
            if (j < 2)
                printf(", ");
        }
        printf(") ");
    }
    printf("\n");

    for (int i = 0; i < 3; i++)
    {
        printf("Texcoord%ds: ", i);
        for (int j = 0; j < 3; j++)
        {
            printf("(%f %f) ", ctx.vertex_queue[j].texcoords[i][0].ToFloat32(), ctx.vertex_queue[j].texcoords[i][1].ToFloat32());
        }
        printf("\n");
    }

    printf("Viewport: (%f, %f) (%f, %f)\n",
           ctx.viewport_width.ToFloat32(), ctx.viewport_height.ToFloat32(),
           ctx.viewport_invw.ToFloat32(), ctx.viewport_invh.ToFloat32());

    Vertex unsortedVerts[3]; // vertices in the order they were sent to GS
    unsortedVerts[0] = ctx.vertex_queue[2];
    unsortedVerts[1] = ctx.vertex_queue[1];
    unsortedVerts[2] = ctx.vertex_queue[0];

    /*// fast reject - some games like to spam triangles that don't have any pixels
    if(unsortedVerts[0].y == unsortedVerts[1].y && unsortedVerts[1].y == unsortedVerts[2].y)
    {
        return;
    }*/


    // sort the three vertices by their y coordinate (increasing)
    uint8_t order[3];
    order3(unsortedVerts[0].pos[1].ToFloat32(), unsortedVerts[1].pos[1].ToFloat32(), unsortedVerts[2].pos[1].ToFloat32(), order);

    // convert all vertex data to floating point, converts position to floating point pixels
    Vertex v0(unsortedVerts[order[0]]);
    Vertex v1(unsortedVerts[order[1]]);
    Vertex v2(unsortedVerts[order[2]]);

    // COMMONLY USED VALUES

    // check if we only have a single triangle like this:
    //     v0  * ----*  v1         v1 *-----* v0
    //                        OR
    //             * v2                 * v2
    // the other orientations of single triangle (where v1 v2 is horizontal) works fine.
    bool lower_tri_only = (v0.pos[1] == v1.pos[1]);

    // the edge e21 is the edge from v1 -> v2.  So v1 + e21 = v2
    // edges (difference of the ENTIRE vertex properties, not just position)
    Vertex e21 = v2 - v1;
    Vertex e20 = v2 - v0;
    Vertex e10 = v1 - v0;

    // interpolating z (or any value) at point P in a triangle can be done by computing the barycentric coordinates
    // (w0, w1, w2) and using P_z = w0 * v1_z + w1 * v2_z + w2 * v3_z

    // derivative of P_z wrt x and y is constant everywhere
    // dP_z/dx = dw0/dx * v1_z + dw1/dx * v2_z + dw2/dx * v3_z

    // w0 = (v2_y - v3_y)*(P_x - v3_x) + (v3_x - v2_x) * (P_y - v3_y)
    //      ----------------------------------------------------------
    //      (v1_y - v0_y)*(v2_x - v0_x) + (v1_x - v0_x)*(v2_y - v0_y)

    // dw0/dx =           v2_y - v3_y
    //          ------------------------------
    //           the same denominator as above

    // The denominator of this fraction shows up everywhere, so we compute it once.
    float24 div = (e10.pos[1] * e20.pos[0] - e10.pos[0] * e20.pos[1]);

    // If the vertices of the triangle are CCW, the denominator will be negative
    // if the triangle is degenerate (has 0 area), it will be zero.
    bool reversed = div.ToFloat32() < 0.f;

    if (div.ToFloat32() == 0.f)
    {
        return;
    }

    /*// next we need to determine the horizontal scanlines that will have pixels.
    // GS pixel draw condition for scissor
    //   >= minimum value, <= maximum value (draw left/top, but not right/bottom)
    // Our scanline loop
    //   >= minimum value, < maximum value

    // MINIMUM SCISSOR
    // -------------------  y = 0.0 (pixel)
    //
    //  XXXXXXXXXXXXXXXXXX  scissor minimum (y = 0.125 to y = 0.875)
    //                           (round to y = 1.0 - the first scanline we should consider)
    // -------------------- y = 1.0 (pixel)
    int scissorY1 = (current_ctx->scissor.y1 + 15) / 16; // min y coordinate, round up because we don't draw px below scissor
    int scissorX1 = (current_ctx->scissor.x1 + 15) / 16;

    // MAXIMUM SCISSOR
    // -------------------  y = 3.0 (pixel)
    //
    //  XXXXXXXXXXXXXXXXXX  scissor maximum (y = 3.125 to y = 3.875)
    //                           (round to y = 4.0 - will do scanlines at y = 1, 2, 3)
    // -------------------- y = 4.0 (pixel)

    // however, if SCISSOR = 4, we should round that up to 5 because we do want to draw pixels on y = 4 (<= max value)
    int scissorY2 = (current_ctx->scissor.y2 + 16) / 16;
    int scissorX2 = (current_ctx->scissor.x2 + 16) / 16;

    // scissor triangle top/bottoms
    // we can get away with only checking min scissor for tops and max scissors for bottom
    // because it will give negative height triangles for completely scissored half tris
    // and the correct answer for half tris that aren't completely killed
    int upperTop = std::max((int)std::ceil(v0.y), scissorY1); // we draw this
    int upperBot = std::min((int)std::ceil(v1.y), scissorY2); // we don't draw this, (< max value, different from scissor)
    int lowerTop = std::max((int)std::ceil(v1.y), scissorY1); // we draw this
    int lowerBot = std::min((int)std::ceil(v2.y), scissorY2); // we don't draw this, (< max value, different from scissor)*/

    int upperTop = (int)std::ceil(v0.pos[1].ToFloat32());
    int upperBot = (int)std::ceil(v1.pos[1].ToFloat32());
    int lowerTop = (int)std::ceil(v1.pos[1].ToFloat32());
    int lowerBot = (int)std::ceil(v2.pos[1].ToFloat32());

    // compute the derivatives of the weights, like shown in the formula above
    float24 ndw2dy = e10.pos[0] / div; // n is negative
    float24 dw2dx  = e10.pos[1] / div;
    float24 dw1dy  = e20.pos[0] / div;
    float24 ndw1dx = e20.pos[1] / div; // also negative

    // derivatives wrt x and y would normally be computed as dz/dx = dw0/dx * z0 + dw1/dx * z1 + dw2/dx * z2
    // however, w0 + w1 + w2 = 1 so dw0/dx + dw1/dx + dw2/dx = 0,
    //   and we can use some clever rearranging and reuse of the edges to simplify this
    //   we can replace dw0/dx with (-dw1/dx - dw2/dx):

    // dz/dx = dw0/dx*z0 + dw1/dx*z1 + dw2/dx*z2
    // dz/dx = (-dw1/dx - dw2/dx)*z0 + dw1/dx*z1 + dw2/dx*z2
    // dz/dx = -dw1/dx*z0 - dw2/dx*z0 + dw1/dx*z1 + dw2/dx*z2
    // dz/dx = dw1/dx*(z1 - z0) + dw2/dx*(z2 - z0)

    // the value to step per field per x/y pixel
    Vertex dvdx = e20 * dw2dx - e10 * ndw1dx;
    Vertex dvdy = e10 * dw1dy - e20 * ndw2dy;

    // slopes of the edges
    float24 e20dxdy = e20.pos[0] / e20.pos[1];
    float24 e21dxdy = e21.pos[0] / e21.pos[1];
    float24 e10dxdy = e10.pos[0] / e10.pos[1];

    // we need to know the left/right side slopes. They can be different if v1 is on the opposite side of e20
    float24 lowerLeftEdgeStep  = reversed ? e20dxdy : e21dxdy;
    float24 lowerRightEdgeStep = reversed ? e21dxdy : e20dxdy;

    // draw triangles
    if(lower_tri_only)
    {
        if(lowerTop < lowerBot) // if we weren't killed by scissoring
        {
            // we don't know which vertex is on the left or right, but the two configures have opposite sign areas:
            //     v0  * ----*  v1         v1 *-----* v0
            //                        OR
            //             * v2                 * v2
            Vertex& left_vertex = reversed ? v0 : v1;
            Vertex& right_vertex = reversed ? v1 : v0;
            rasterize_half_tri(left_vertex.pos[0],    // upper edge left vertex, floating point pixels
                                 right_vertex.pos[0],   // upper edge right vertex, floating point pixels
                                 upperTop,         // start scanline (included)
                                 lowerBot,         // end scanline   (not included)
                                 dvdx,             // derivative of values wrt x coordinate
                                 dvdy,             // derivative of values wrt y coordinate
                                 left_vertex,      // one point to interpolate from
                                 lowerLeftEdgeStep, // slope of left edge
                                 lowerRightEdgeStep  // slope of right edge
                                );
        }
    }
    else
    {
        // again, left/right slopes
        float24 upperLeftEdgeStep = reversed ? e20dxdy : e10dxdy;
        float24 upperRightEdgeStep = reversed ? e10dxdy : e20dxdy;

        // upper triangle
        if(upperTop < upperBot) // if we weren't killed by scissoring
        {
            rasterize_half_tri(v0.pos[0], v0.pos[0],          // upper edge is just the highest point on triangle
                                 upperTop, upperBot,  // scanline bounds
                                 dvdx, dvdy,          // derivatives of values
                                 v0,                  // interpolate from this vertex
                                 upperLeftEdgeStep, upperRightEdgeStep // slopes
                                 );
        }

        if(lowerTop < lowerBot)
        {
            //             * v0
            //
            //     v1  * ----
            //
            //
            //               * v2
            rasterize_half_tri(v0.pos[0] + upperLeftEdgeStep * e10.pos[1], // one of our upper edge vertices isn't v0,v1,v2, but we don't know which. todo is this faster than branch?
                                 v0.pos[0] + upperRightEdgeStep * e10.pos[1],
                                 lowerTop, lowerBot, dvdx, dvdy, v1,
                                 lowerLeftEdgeStep, lowerRightEdgeStep);
        }

    }

}

/*!
 * Render a "half-triangle" which has a horizontal edge
 * @param x0 - the x coordinate of the upper left most point of the triangle. floating point pixels
 * @param x1 - the x coordinate of the upper right most point of the triangle (can be the same as x0), floating point px
 * @param y0 - the y coordinate of the first scanline which will contain the triangle (integer pixels)
 * @param y1 - the y coordinate of the last scanline which will contain the triangle (integer pixels)
 * @param x_step - the derivatives of all parameters wrt x
 * @param y_step - the derivatives of all parameters wrt y
 * @param init   - the vertex we interpolate from
 * @param step_x0 - how far to step to the left on each step down (floating point px)
 * @param step_x1 - how far to step to the right on each step down (floating point px)
 * @param scx1    - left x scissor (fp px)
 * @param scx2    - right x scissor (fp px)
 * @param tex_info - texture data
 */
void GPU::rasterize_half_tri(float24 x0, float24 x1, int y0, int y1, Vertex &x_step,
                                  Vertex &y_step, Vertex &init, float24 step_x0, float24 step_x1) {
    for(int y = y0; y < y1; y++) // loop over scanlines of triangle
    {
        float24 height = float24::FromFloat32(y) - init.pos[1]; // how far down we've made it
        Vertex vtx = init + y_step * height;       // interpolate to point (x_init, y)
        float24 x0l = x0 + step_x0 * height;          // start x coordinates of scanline from interpolation
        float24 x1l = x1 + step_x1 * height;          // end   x coordinate of scanline from interpolation
        /*x0l = std::max(scx1, std::ceil(x0l));       // round and scissor
        x1l = std::min(scx2, std::ceil(x1l));       // round and scissor*/
        int xStop = x1l.ToFloat32();                            // integer start/stop pixels
        int xStart = x0l.ToFloat32();

        if(xStop == xStart) continue;               // skip rows of zero length

        vtx += (x_step * (x0l - init.pos[0]));           // interpolate to point (x0l, y)

        for (int x = x0l.ToFloat32(); x < xStop; x++)            // loop over x pixels of scanline
        {
            RGBA_Color source_color, frame_color;

            source_color.r = (uint8_t)(vtx.color[0].ToFloat32() * 255.0);
            source_color.g = (uint8_t)(vtx.color[1].ToFloat32() * 255.0);
            source_color.b = (uint8_t)(vtx.color[2].ToFloat32() * 255.0);
            source_color.a = (uint8_t)(vtx.color[3].ToFloat32() * 255.0);

            combine_textures(source_color, vtx);

            uint32_t addr = get_swizzled_tile_addr(ctx.color_buffer_base, ctx.frame_width, x, y, 4);
            uint32_t frame = bswp32(e->arm11_read32(0, addr));

            frame_color.r = frame & 0xFF;
            frame_color.g = (frame >> 8) & 0xFF;
            frame_color.b = (frame >> 16) & 0xFF;
            frame_color.a = frame >> 24;

            blend_fragment(source_color, frame_color);

            uint32_t final_color = 0;
            final_color = source_color.r | (source_color.g << 8) | (source_color.b << 16) | (source_color.a << 24);

            e->arm11_write32(0, addr, bswp32(final_color));

            vtx += x_step;                       // get values for the adjacent pixel
        }
    }
}

void GPU::tex_lookup(int index, RGBA_Color &tex_color, Vertex &vtx)
{
    tex_color.r = 0;
    tex_color.g = 0;
    tex_color.b = 0;
    tex_color.a = 0;

    //TODO: A NULL/disabled texture should return the most recently rendered fragment color
    if (!ctx.tex_enable[index])
        return;

    int height = ctx.tex_height[index];
    int width = ctx.tex_width[index];
    int u = roundf(vtx.texcoords[index][0].ToFloat32() * ctx.tex_width[index]);
    int v = roundf(vtx.texcoords[index][1].ToFloat32() * ctx.tex_height[index]);

    switch (ctx.tex_swrap[index])
    {
        case 0:
            u = std::max(0, u);
            u = std::min(width - 1, u);
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
        case 0x4:
            //RGBA4444
            addr = get_swizzled_tile_addr(addr, width, u, v, 2);
            texel = bswp16(e->arm11_read16(0, addr));

            tex_color.r = (texel & 0xF) << 4;
            tex_color.g = ((texel >> 4) & 0xF) << 4;
            tex_color.b = ((texel >> 8) & 0xF) << 4;
            tex_color.a = (texel >> 12) << 4;
            break;
        case 0x5:
            //IA8
            addr = get_swizzled_tile_addr(addr, width, u, v, 2);
            texel = bswp16(e->arm11_read16(0, addr));

            //TODO: When alpha is disabled, R should be intensity and G should be alpha
            tex_color.r = texel & 0xFF;
            tex_color.g = texel & 0xFF;
            tex_color.b = texel & 0xFF;
            tex_color.a = texel >> 8;
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
            uint8_t i = (texel >> 4) << 4;
            uint8_t a = (texel & 0xF) << 4;

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

            tex_color.r = texel << 4;
            tex_color.g = texel << 4;
            tex_color.b = texel << 4;
            tex_color.a = 0xFF;
            break;
        case 0xB:
            //A4
            addr = get_4bit_swizzled_addr(addr, width, u, v);
            if (u & 0x1)
                texel = e->arm11_read8(0, addr) >> 4;
            else
                texel = e->arm11_read8(0, addr) & 0xF;

            tex_color.a = texel << 4;
            break;
        default:
            printf("[GPU] Unrecognized tex format $%02X\n", ctx.tex_type[index]);
    }
}

void GPU::get_tex0(RGBA_Color &tex_color, Vertex &vtx)
{
    tex_lookup(0, tex_color, vtx);
}

void GPU::get_tex1(RGBA_Color &tex_color, Vertex &vtx)
{
    tex_lookup(1, tex_color, vtx);
}

void GPU::get_tex2(RGBA_Color &tex_color, Vertex &vtx)
{
    tex_lookup(2, tex_color, vtx);
}

void GPU::combine_textures(RGBA_Color &source, Vertex& vtx)
{
    RGBA_Color primary = source;
    RGBA_Color prev = source;
    RGBA_Color comb_buffer = {0, 0, 0, 0};

    RGBA_Color tex[4];

    get_tex0(tex[0], vtx);
    get_tex1(tex[1], vtx);
    get_tex2(tex[2], vtx);

    for (int i = 0; i < 6; i++)
    {
        RGBA_Color sources[3], operands[3];

        for (int j = 0; j < 3; j++)
        {
            switch (ctx.texcomb_rgb_source[i][j])
            {
                case 0x0:
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

            if (ctx.texcomb_alpha_source[i][j] != ctx.texcomb_rgb_source[i][j])
            {
                switch (ctx.texcomb_alpha_source[i][j])
                {
                    case 0x0:
                        sources[j].a = primary.a;
                        break;
                    case 0x3:
                        sources[j].a = tex[0].a;
                        break;
                    case 0x4:
                        sources[j].a = tex[1].a;
                        break;
                    case 0x5:
                        sources[j].a = tex[2].a;
                        break;
                    case 0xD:
                        sources[j].a = comb_buffer.a;
                        break;
                    case 0xE:
                        sources[j].a = ctx.texcomb_const[i].a;
                        break;
                    case 0xF:
                        sources[j].a = prev.a;
                        break;
                    default:
                        EmuException::die("[GPU] Unrecognized texcomb alpha source $%02X",
                                          ctx.texcomb_alpha_source[i][j]);
                }
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
                    operands[j].r = sources[j].a;
                    operands[j].g = sources[j].a;
                    operands[j].b = sources[j].a;
                    break;
                case 0x3:
                    operands[j].r = 255 - sources[j].a;
                    operands[j].g = 255 - sources[j].a;
                    operands[j].b = 255 - sources[j].a;
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
                    operands[j].a = sources[j].a;
                    break;
                case 0x1:
                    operands[j].a = 255 - sources[j].a;
                    break;
                case 0x2:
                    operands[j].a = sources[j].r;
                    break;
                case 0x3:
                    operands[j].a = 255 - sources[j].r;
                    break;
                case 0x4:
                    operands[j].a = sources[j].g;
                    break;
                case 0x5:
                    operands[j].a = 255 - sources[j].g;
                    break;
                case 0x6:
                    operands[j].a = sources[j].b;
                    break;
                case 0x7:
                    operands[j].a = 255 - sources[j].b;
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

        comb_buffer = ctx.texcomb_buffer;

        if (ctx.texcomb_rgb_buffer_update[i])
        {
            ctx.texcomb_buffer.r = prev.r;
            ctx.texcomb_buffer.g = prev.g;
            ctx.texcomb_buffer.b = prev.b;
        }

        if (ctx.texcomb_alpha_buffer_update[i])
            ctx.texcomb_buffer.a = prev.a;
    }

    source = prev;
}

void GPU::blend_fragment(RGBA_Color &source, RGBA_Color &frame)
{
    int source_alpha = source.a;
    int dest_alpha = frame.a;
    switch (ctx.fragment_op)
    {
        case 0:
            //Regular blending
            if (ctx.blend_mode == 0)
                EmuException::die("[GPU] Logical blending not implemented");

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
                    source.r *= source.r;
                    source.g *= source.g;
                    source.b *= source.b;
                    break;
                case 0x6:
                    //Source alpha
                    source.r *= source_alpha;
                    source.g *= source_alpha;
                    source.b *= source_alpha;
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
                    source.a *= source_alpha;
                    break;
                case 0x6:
                    source.a *= source_alpha;
                    break;
                case 0x9:
                    source.a *= dest_alpha;
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
                    frame.r *= frame.r;
                    frame.g *= frame.g;
                    frame.b *= frame.b;
                    break;
                case 0x7:
                    frame.r *= 255 - source_alpha;
                    frame.g *= 255 - source_alpha;
                    frame.b *= 255 - source_alpha;
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
                    frame.a *= dest_alpha;
                    break;
                case 0x7:
                    frame.a *= 255 - source_alpha;
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
            break;
        default:
            EmuException::die("[GPU] Unrecognized fragment operation %d", ctx.fragment_op);
    }
}

void GPU::exec_shader(ShaderUnit& sh)
{
    //Prepare input registers
    for (int i = 0; i < sh.total_inputs; i++)
    {
        int mapping = sh.input_mapping[i];
        sh.input_regs[mapping] = sh.input_attrs[i];
    }

    for (int i = 0; i < sh.total_inputs; i++)
    {
        printf("v%d: ", i);
        for (int j = 0; j < 4; j++)
            printf("%f ", sh.input_regs[i][j].ToFloat32());
        printf("\n");
    }

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
    sh.pc = sh.entry_point;

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
            case 0x02:
                shader_dp4(sh, instr);
                break;
            case 0x08:
                shader_mul(sh, instr);
                break;
            case 0x0C:
                shader_max(sh, instr);
                break;
            case 0x12:
                shader_mova(sh, instr);
                break;
            case 0x13:
                shader_mov(sh, instr);
                break;
            case 0x21:
                //NOP
                break;
            case 0x22:
                ended = true;
                break;
            case 0x24:
                shader_call(sh, instr);
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
            case 0x2E:
            case 0x2F:
                shader_cmp(sh, instr);
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
                sh.loop_ctr_reg += sh.loop_inc_reg;

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

    for (int i = 0; i < 7; i++)
    {
        printf("o%d: ", i);
        for (int j = 0; j < 4; j++)
            printf("%f ", sh.output_regs[i][j].ToFloat32());
        printf("\n");
    }
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
            negate = (op_desc >> 22) & 0x3;
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

uint8_t GPU::get_idx1(ShaderUnit& sh, uint8_t idx1, uint8_t src1)
{
    if (src1 < 0x20)
        return 0;

    switch (idx1)
    {
        case 0:
            return 0;
        case 1:
            return (uint8_t)sh.addr_reg[0].ToFloat32();
        case 2:
            return (uint8_t)sh.addr_reg[1].ToFloat32();
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

    float24 dot_product = float24::FromFloat32(0.0f);

    for (int i = 0; i < 4; i++)
        dot_product += src[0][i] * src[1][i];

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

void GPU::shader_call(ShaderUnit &sh, uint32_t instr)
{
    uint8_t num = instr & 0xFF;

    uint16_t dst = (instr >> 10) & 0xFFF;

    sh.call_stack[sh.call_ptr] = sh.pc;
    sh.call_cmp_stack[sh.call_ptr] = (dst + num) * 4;
    sh.call_ptr++;

    sh.pc = dst * 4;
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
    sh.loop_inc_reg = sh.int_regs[int_id][2];

    sh.loop_ptr++;
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
    cmp_ops[0] = (instr >> 21) & 0x7;
    cmp_ops[1] = (instr >> 24) & 0x7;

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
        case 0x70:
            return fb->color_format;
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
