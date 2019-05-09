#ifndef GPU_HPP
#define GPU_HPP
#include <cstdint>

struct FrameBuffer
{
    uint32_t left_addr_a, left_addr_b;
    uint32_t right_addr_a, right_addr_b;

    uint8_t color_format;
};

struct MemoryFill
{
    uint32_t start;
    uint32_t end;
    uint32_t value;

    uint8_t fill_width;
    bool finished;
};

class GPU
{
    private:
        uint8_t* vram;

        uint8_t* top_screen, *bottom_screen;

        FrameBuffer framebuffers[2];

        MemoryFill memfill[2];

        uint32_t read32_fb(int index, uint32_t addr);
        void write32_fb(int index, uint32_t addr, uint32_t value);

        void render_fb_pixel(uint8_t* screen, int fb_index, int x, int y);
    public:
        GPU();
        ~GPU();

        void reset();
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
