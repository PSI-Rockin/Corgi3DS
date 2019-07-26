#ifndef GPU_HPP
#define GPU_HPP
#include <cstdint>
#include "gpu_floats.hpp"
#include "vector_math.hpp"

struct FrameBuffer
{
    uint32_t left_addr_a, left_addr_b;
    uint32_t right_addr_a, right_addr_b;

    uint8_t color_format;
    bool buffer_select;
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
    bool cmp_regs[2];

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
    uint8_t code[512 * sizeof(uint32_t)];

    uint32_t op_desc_index;
    uint32_t op_desc[128];

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
};

struct GPU_Context
{
    //A generic representation of all registers, used for masking
    uint32_t regs[0x300];

    //Rasterizer registers
    float24 viewport_width, viewport_height;
    float24 viewport_invw, viewport_invh;
    int16_t viewport_x, viewport_y;

    uint8_t vsh_output_total;
    uint8_t vsh_output_mapping[7][4];

    //Framebuffer registers
    uint32_t depth_buffer_base;
    uint32_t color_buffer_base;
    uint16_t frame_width, frame_height;

    //Geometry pipeline
    Vertex vertex_queue[3];
    int submitted_vertices;

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

    uint8_t fixed_attr_index;
    uint32_t fixed_attr_buffer[3];
    int fixed_attr_count;

    uint8_t prim_mode;

    //Geometry shader pipeline
    ShaderUnit gsh;

    //Vertex shader pipeline
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

        FrameBuffer framebuffers[2];

        MemoryFill memfill[2];

        TransferEngine dma;
        CommandEngine cmd_engine;

        GPU_Context ctx;

        uint32_t read32_fb(int index, uint32_t addr);
        void write32_fb(int index, uint32_t addr, uint32_t value);

        void do_transfer_engine_dma(uint64_t param);
        void do_command_engine_dma(uint64_t param);
        void do_memfill(int index);

        void write_cmd_register(int reg, uint32_t param, uint8_t mask);

        void draw_array_elements();
        void rasterize_tri();

        //Shader ops
        void exec_shader(ShaderUnit& sh);
        Vec4<float24> swizzle_sh_src(Vec4<float24> src, uint32_t op_desc, int src_type);
        Vec4<float24> get_src(ShaderUnit& sh, uint8_t src);
        uint8_t get_idx1(uint8_t idx1, uint8_t src1);
        void set_sh_dest(ShaderUnit& sh, uint8_t dst, float24 value, int index);

        void shader_dp4(ShaderUnit& sh, uint32_t instr);
        void shader_mul(ShaderUnit& sh, uint32_t instr);
        void shader_mov(ShaderUnit& sh, uint32_t instr);
        void shader_call(ShaderUnit& sh, uint32_t instr);
        void shader_ifu(ShaderUnit& sh, uint32_t instr);
        void shader_ifc(ShaderUnit& sh, uint32_t instr);
        void shader_cmp(ShaderUnit& sh, uint32_t instr);

        void render_fb_pixel(uint8_t* screen, int fb_index, int x, int y);
    public:
        GPU(Emulator* e, Scheduler* scheduler, MPCore_PMR* pmr);
        ~GPU();

        void reset(uint8_t* vram);
        void render_frame();

        template <typename T> T read_vram(uint32_t addr);
        template <typename T> void write_vram(uint32_t addr, T value);

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);

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
