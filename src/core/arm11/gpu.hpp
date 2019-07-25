#ifndef GPU_HPP
#define GPU_HPP
#include <cstdint>

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

        uint32_t read32_fb(int index, uint32_t addr);
        void write32_fb(int index, uint32_t addr, uint32_t value);

        void do_transfer_engine_dma(uint64_t param);
        void do_command_engine_dma(uint64_t param);
        void do_memfill(int index);

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
