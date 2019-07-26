#include <cstdio>
#include <cstring>
#include "gpu.hpp"
#include "signextend.hpp"
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

    framebuffers[0].left_addr_a = 0x18000000;
    framebuffers[1].left_addr_a = 0x18000000;
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
            color = e->arm11_read32(0, start + (index * 4));
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
                EmuException::die("[GPU] Immediate mode submission!");
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
        case 0x22F:
            if (param != 0)
                draw_array_elements();
            break;
        case 0x232:
            ctx.fixed_attr_index = param & 0xF;
            ctx.fixed_attr_count = 0;
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

void GPU::draw_array_elements()
{
    printf("[GPU] DRAW_ARRAY_ELEMENTS\n");
    uint32_t index_base = ctx.vtx_buffer_base + ctx.index_buffer_offs;
    uint32_t index_offs = 0;

    uint64_t vtx_fmts = ctx.attr_buffer_format_low;
    vtx_fmts |= (uint64_t)ctx.attr_buffer_format_hi << 32ULL;
    for (unsigned int i = 0; i < ctx.vertices; i++)
    {
        uint16_t index;
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

        //Run the vertex shader
        exec_shader(ctx.vsh);

        //TODO: Run geometry shader?

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

        //Apply viewport transform
        v.pos[0] += float24::FromFloat32(1.0f);
        v.pos[0] = (v.pos[0] * ctx.viewport_width) + float24::FromFloat32(ctx.viewport_x);

        v.pos[1] += float24::FromFloat32(1.0f);
        v.pos[1] = (v.pos[1] * ctx.viewport_height) + float24::FromFloat32(ctx.viewport_y);

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
                default:
                    EmuException::die("[GPU] Unrecognized primitive mode %d", ctx.prim_mode);
            }
        }
    }
}

void GPU::rasterize_tri()
{
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

    printf("Viewport: (%f, %f) (%f, %f)\n",
           ctx.viewport_width.ToFloat32(), ctx.viewport_height.ToFloat32(),
           ctx.viewport_invw.ToFloat32(), ctx.viewport_invh.ToFloat32());
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

    sh.call_ptr = 0;
    sh.if_ptr = 0;
    sh.pc = sh.entry_point;

    bool ended = false;
    while (!ended)
    {
        uint32_t instr = *(uint32_t*)&sh.code[sh.pc];
        printf("[GPU] [$%04X] $%08X\n", sh.pc, instr);
        sh.pc += 4;

        switch (instr >> 26)
        {
            case 0x02:
                shader_dp4(sh, instr);
                break;
            case 0x08:
                shader_mul(sh, instr);
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
            case 0x27:
                shader_ifu(sh, instr);
                break;
            case 0x28:
                shader_ifc(sh, instr);
                break;
            case 0x2E:
            case 0x2F:
                shader_cmp(sh, instr);
                break;
            default:
                EmuException::die("[GPU] Unrecognized shader opcode $%02X (instr:$%08X pc:$%04X)",
                                  instr >> 26, instr, sh.pc - 4);
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

uint8_t GPU::get_idx1(uint8_t idx1, uint8_t src1)
{
    if (src1 < 0x20)
        return 0;

    switch (idx1)
    {
        case 0:
            return 0;
        default:
            EmuException::die("[GPU] IDX1 used");
            return 0;
    }
}

void GPU::set_sh_dest(ShaderUnit &sh, uint8_t dst, float24 value, int index)
{
    printf("[GPU] Setting sh reg $%02X:%d to %f\n", dst, index, value.ToFloat32());
    if (dst < 0x7)
        sh.output_regs[dst][index] = value;
    else if (dst >= 0x10 && dst < 0x20)
        sh.temp_regs[dst - 0x10][index] = value;
    else
        EmuException::die("[GPU] Unrecognized dst $%02X in set_sh_dest", dst);
}

void GPU::shader_dp4(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(idx1, src1);

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

    idx1 = get_idx1(idx1, src1);

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

void GPU::shader_mov(ShaderUnit& sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;
    uint8_t dest = (instr >> 21) & 0x1F;

    idx1 = get_idx1(idx1, src1);

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

void GPU::shader_cmp(ShaderUnit &sh, uint32_t instr)
{
    uint32_t op_desc = sh.op_desc[instr & 0x7F];

    uint8_t src2 = (instr >> 7) & 0x1F;
    uint8_t src1 = (instr >> 12) & 0x7F;
    uint8_t idx1 = (instr >> 19) & 0x3;

    idx1 = get_idx1(idx1, src1);

    src1 += idx1;

    Vec4<float24> src[2];

    src[0] = swizzle_sh_src(get_src(sh, src1), op_desc, 1);
    src[1] = swizzle_sh_src(get_src(sh, src2), op_desc, 2);

    uint8_t cmp_ops[2];
    cmp_ops[0] = (instr >> 21) & 0x7;
    cmp_ops[1] = (instr >> 24) & 0x7;

    for (int i = 0; i < 2; i++)
    {
        printf("[GPU] Comparing %f to %f\n", src[0][i].ToFloat32(), src[1][i].ToFloat32());
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

        printf("[GPU] Result of cmp op: %d\n", sh.cmp_regs[i]);
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
