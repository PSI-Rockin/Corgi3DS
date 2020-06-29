#ifndef GPU_HPP
#define GPU_HPP
#include <cstdint>
#include "gpu_floats.hpp"
#include "vector_math.hpp"

struct RGBA_Color
{
    int32_t r, g, b, a;
};

struct FrameBuffer
{
    uint32_t left_addr_a, left_addr_b;
    uint32_t right_addr_a, right_addr_b;
    uint32_t stride;

    uint16_t total_scanlines, scanlines;
    int scanline_timing;

    uint8_t color_format;
    bool buffer_select;

    uint32_t screenfill_color;
    bool screenfill_enabled;
};

struct MemoryFill
{
    uint32_t start;
    uint32_t end;
    uint32_t value;

    uint8_t fill_width;
    bool busy;
    bool finished;
};

struct TransferEngine
{
    uint32_t input_addr;
    uint32_t output_addr;
    uint32_t disp_input_width, disp_input_height;
    uint32_t disp_output_width, disp_output_height;
    uint32_t flags;

    uint32_t tc_size;
    uint32_t tc_input_width, tc_output_width;
    uint32_t tc_input_gap, tc_output_gap;

    bool busy;
    bool finished;
};

struct CommandEngine
{
    uint32_t size;
    uint32_t input_addr;
    bool busy;
};

struct ShaderUnit
{
    //NOTE: input_regs is input_attrs done after input_mapping is applied in a shader program!
    Vec4<float24> input_regs[16];
    Vec4<float24> temp_regs[16];
    Vec4<float24> output_regs[16];
    Vec4<float24> float_uniform[96];
    uint16_t bool_uniform;
    uint8_t int_regs[4][4];
    bool cmp_regs[2];
    Vec2<float24> addr_reg;

    uint8_t total_inputs;

    Vec4<float24> input_attrs[16];
    uint8_t float_uniform_index;
    bool float_uniform_mode;
    uint32_t float_uniform_buffer[16];
    int float_uniform_counter;

    uint16_t entry_point;
    uint16_t pc;

    uint8_t input_mapping[16];

    uint32_t code_index;
    uint8_t code[4096 * sizeof(uint32_t)];

    uint32_t op_desc_index;
    uint32_t op_desc[128];

    uint16_t loop_cmp_stack[4];
    uint16_t loop_stack[4];
    uint8_t loop_iter_stack[4];
    uint8_t loop_ptr;
    uint32_t loop_ctr_reg;
    uint32_t loop_inc_reg[4];

    uint16_t if_cmp_stack[8];
    uint16_t if_stack[8];
    uint8_t if_ptr;

    uint16_t call_cmp_stack[4];
    uint16_t call_stack[4];
    uint8_t call_ptr;
};

struct Vertex
{
    Vec4<float24> pos;
    Vec4<float24> quat;
    Vec4<float24> color;
    Vec4<float24> texcoords[3];
    Vec4<float24> view;

    Vertex operator-(const Vertex& rhs)
    {
        Vertex result;
        result.pos = pos - rhs.pos;
        result.color = color - rhs.color;
        result.texcoords[0] = texcoords[0] - rhs.texcoords[0];
        result.texcoords[1] = texcoords[1] - rhs.texcoords[1];
        result.texcoords[2] = texcoords[2] - rhs.texcoords[2];
        return result;
    }

    Vertex operator+(const Vertex& rhs)
    {
        Vertex result;
        result.pos = pos + rhs.pos;
        result.color = color + rhs.color;
        result.texcoords[0] = texcoords[0] + rhs.texcoords[0];
        result.texcoords[1] = texcoords[1] + rhs.texcoords[1];
        result.texcoords[2] = texcoords[2] + rhs.texcoords[2];
        return result;
    }

    Vertex& operator+=(const Vertex& rhs)
    {
        pos += rhs.pos;
        color += rhs.color;
        texcoords[0] += rhs.texcoords[0];
        texcoords[1] += rhs.texcoords[1];
        texcoords[2] += rhs.texcoords[2];
        return *this;
    }

    Vertex operator*(float24 mult)
    {
        Vertex result;
        result.pos = pos * mult;
        result.color = color * mult;
        result.texcoords[0] = texcoords[0] * mult;
        result.texcoords[1] = texcoords[1] * mult;
        result.texcoords[2] = texcoords[2] * mult;
        return result;
    }
};

struct GPU_Context
{
    //A generic representation of all registers, used for masking
    uint32_t regs[0x300];

    //Rasterizer registers
    uint8_t cull_mode;
    float24 viewport_width, viewport_height;
    float24 viewport_invw, viewport_invh;
    int16_t viewport_x, viewport_y;

    float24 depth_scale, depth_offset;

    uint8_t sh_output_total;
    uint8_t sh_output_mapping[7][4];

    bool use_z_for_depth;

    //Texturing registers
    bool tex_enable[4];
    uint8_t tex3_coords;
    bool tex2_uses_tex1_coords;
    RGBA_Color tex_border[3];
    uint32_t tex_width[3];
    uint32_t tex_height[3];
    uint8_t tex_swrap[3];
    uint8_t tex_twrap[3];
    uint8_t tex0_type;
    uint32_t tex_addr[3];
    uint32_t tex0_addr[5];
    uint8_t tex_type[3];

    int texcomb_start, texcomb_end;
    uint8_t texcomb_rgb_source[6][3];
    uint8_t texcomb_alpha_source[6][3];
    uint8_t texcomb_rgb_operand[6][3];
    uint8_t texcomb_alpha_operand[6][3];
    uint8_t texcomb_rgb_op[6];
    uint8_t texcomb_alpha_op[6];
    RGBA_Color texcomb_const[6];
    uint8_t texcomb_rgb_scale[6];
    uint8_t texcomb_alpha_scale[6];

    bool texcomb_rgb_buffer_update[6];
    bool texcomb_alpha_buffer_update[6];
    RGBA_Color texcomb_buffer;

    //Framebuffer registers
    uint8_t fragment_op;
    uint8_t blend_mode;
    RGBA_Color blend_color;

    uint8_t blend_rgb_equation;
    uint8_t blend_alpha_equation;
    uint8_t blend_rgb_src_func;
    uint8_t blend_rgb_dst_func;
    uint8_t blend_alpha_src_func;
    uint8_t blend_alpha_dst_func;

    uint8_t logic_op;

    bool alpha_test_enabled;
    uint8_t alpha_test_func;
    uint8_t alpha_test_ref;

    bool stencil_test_enabled;
    uint8_t stencil_test_func;
    uint8_t stencil_write_mask;
    uint8_t stencil_ref;
    uint8_t stencil_input_mask;

    uint8_t stencil_fail_func;
    uint8_t stencil_depth_fail_func;
    uint8_t stencil_depth_pass_func;

    bool depth_test_enabled;
    uint8_t depth_test_func;
    bool rgba_write_enabled[4];
    bool depth_write_enabled;

    uint8_t allow_stencil_depth_write;

    uint8_t color_format;
    uint8_t depth_format;

    uint32_t depth_buffer_base;
    uint32_t color_buffer_base;
    uint16_t frame_width, frame_height;

    //Geometry pipeline
    Vertex vertex_queue[4];
    int submitted_vertices;
    bool even_strip;

    uint32_t vtx_buffer_base;

    uint32_t attr_buffer_format_low;
    uint32_t attr_buffer_format_hi;
    uint16_t fixed_attr_mask;
    uint8_t total_vtx_attrs;

    uint32_t attr_buffer_offs[12];
    uint32_t attr_buffer_cfg1[12];
    uint16_t attr_buffer_cfg2[12];
    uint8_t attr_buffer_vtx_size[12];
    uint8_t attr_buffer_components[12];

    uint32_t index_buffer_offs;
    bool index_buffer_short;

    uint32_t vertices;
    uint32_t vtx_offset;

    uint8_t fixed_attr_index;
    uint32_t fixed_attr_buffer[3];
    int fixed_attr_count;

    CommandEngine cmd_engine[2];

    uint8_t vsh_inputs;
    uint8_t vsh_input_counter;

    bool gsh_enabled;

    int vsh_outputs;

    uint8_t gsh_mode;
    uint8_t gsh_fixed_vtx_num;
    uint8_t gsh_stride;
    uint8_t gsh_start_index;

    uint8_t prim_mode;

    //Geometry shader
    ShaderUnit gsh;
    Vertex gsh_vtx_buffer[3];
    int gsh_attrs_input;
    int gsh_vtx_id;
    bool gsh_winding;
    bool gsh_emit_prim;

    //Vertex shader
    ShaderUnit vsh;
};

class Emulator;
class Scheduler;
class MPCore_PMR;

class GPU
{
    private:
        Emulator* e;
        Scheduler* scheduler;
        MPCore_PMR* pmr;
        uint8_t* vram;

        uint8_t* top_screen, *bottom_screen;

        bool lcd_initialized;
        FrameBuffer framebuffers[2];

        MemoryFill memfill[2];

        TransferEngine dma;

        uint32_t cur_cmdlist_ptr;
        uint32_t cur_cmdlist_size;
        bool cmd_engine_busy;

        GPU_Context ctx;

        uint32_t read32_fb(int index, uint32_t addr);
        void write32_fb(int index, uint32_t addr, uint32_t value);

        uint32_t get_4bit_swizzled_addr(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint32_t get_swizzled_tile_addr(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t size);

        void do_transfer_engine_dma(uint64_t param);
        void do_command_engine_dma(uint64_t param);
        void do_memfill(int index);

        void start_command_engine_dma(int index);

        void write_cmd_register(int reg, uint32_t param, uint8_t mask);
        void input_float_uniform(ShaderUnit& sh, uint32_t param);

        void draw_vtx_array(bool is_indexed);
        void input_vsh_vtx();
        void map_sh_output_to_vtx(ShaderUnit& sh, Vertex& v);
        void submit_vtx(Vertex& v, bool winding);
        void process_tri(Vertex& v0, Vertex& v1, Vertex& v2);
        void viewport_transform(Vertex& v);

        bool get_fill_rule_bias(Vertex& vtx, Vertex& line1, Vertex& line2);
        void rasterize_tri(Vertex& v0, Vertex& v1, Vertex& v2);
        void rasterize_half_tri(float24 x0, float24 x1, int y0, int y1, Vertex &x_step,
                                Vertex &y_step, Vertex &init, float24 step_x0, float24 step_x1);

        void tex_lookup(int index, int coord_index, RGBA_Color& tex_color, Vertex& vtx);
        void decode_etc1(RGBA_Color& tex_color, int u, int v, uint64_t data);
        void get_tex0(RGBA_Color& tex_color, Vertex& vtx);
        void get_tex1(RGBA_Color& tex_color, Vertex& vtx);
        void get_tex2(RGBA_Color& tex_color, Vertex& vtx);
        void combine_textures(RGBA_Color& source, Vertex& vtx);

        void blend_fragment(RGBA_Color& source, RGBA_Color& frame);
        void do_alpha_blending(RGBA_Color& source, RGBA_Color& frame);
        void do_logic_op(RGBA_Color& source, RGBA_Color& frame);
        void update_stencil(uint32_t addr, uint8_t old, uint8_t ref, uint8_t func);

        //Shader ops
        void exec_shader(ShaderUnit& sh);
        Vec4<float24> swizzle_sh_src(Vec4<float24> src, uint32_t op_desc, int src_type);
        Vec4<float24> get_src(ShaderUnit& sh, uint8_t src);
        int get_idx1(ShaderUnit& sh, uint8_t idx1, uint8_t src1);
        void set_sh_dest(ShaderUnit& sh, uint8_t dst, float24 value, int index);

        bool shader_meets_cond(ShaderUnit& sh, uint32_t instr);
        void shader_add(ShaderUnit& sh, uint32_t instr);
        void shader_dp3(ShaderUnit& sh, uint32_t instr);
        void shader_dp4(ShaderUnit& sh, uint32_t instr);
        void shader_dph(ShaderUnit& sh, uint32_t instr);
        void shader_mul(ShaderUnit& sh, uint32_t instr);
        void shader_max(ShaderUnit& sh, uint32_t instr);
        void shader_rcp(ShaderUnit& sh, uint32_t instr);
        void shader_rsq(ShaderUnit& sh, uint32_t instr);
        void shader_mova(ShaderUnit& sh, uint32_t instr);
        void shader_mov(ShaderUnit& sh, uint32_t instr);
        void shader_slti(ShaderUnit& sh, uint32_t instr);
        void shader_break(ShaderUnit& sh, uint32_t instr);
        void shader_breakc(ShaderUnit& sh, uint32_t instr);
        void shader_call(ShaderUnit& sh, uint32_t instr);
        void shader_callc(ShaderUnit& sh, uint32_t instr);
        void shader_callu(ShaderUnit& sh, uint32_t instr);
        void shader_ifu(ShaderUnit& sh, uint32_t instr);
        void shader_ifc(ShaderUnit& sh, uint32_t instr);
        void shader_loop(ShaderUnit& sh, uint32_t instr);
        void shader_emit(ShaderUnit& sh, uint32_t instr);
        void shader_setemit(ShaderUnit& sh, uint32_t instr);
        void shader_jmpc(ShaderUnit& sh, uint32_t instr);
        void shader_jmpu(ShaderUnit& sh, uint32_t instr);
        void shader_cmp(ShaderUnit& sh, uint32_t instr);
        void shader_madi(ShaderUnit& sh, uint32_t instr);
        void shader_mad(ShaderUnit& sh, uint32_t instr);

        void render_fb_pixel(uint8_t* screen, int fb_index, int x, int y);
        void scanline_event(int index);
    public:
        GPU(Emulator* e, Scheduler* scheduler, MPCore_PMR* pmr);
        ~GPU();

        void reset(uint8_t* vram);
        void render_frame();

        template <typename T> T read_vram(uint32_t addr);
        template <typename T> void write_vram(uint32_t addr, T value);

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);

        void set_lcd_init(bool init);
        void set_screenfill(int index, uint32_t value);

        uint8_t* get_top_buffer();
        uint8_t* get_bottom_buffer();
};

template <typename T>
inline T GPU::read_vram(uint32_t addr)
{
    return *(T*)&vram[addr % 0x00600000];
}

template <typename T>
inline void GPU::write_vram(uint32_t addr, T value)
{
    *(T*)&vram[addr % 0x00600000] = value;
}

#endif // GPU_HPP
