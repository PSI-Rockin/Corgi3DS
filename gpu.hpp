#ifndef GPU_HPP
#define GPU_HPP
#include <cstdint>

struct FrameBuffer
{
    uint32_t left_addr_a, left_addr_b;
    uint32_t right_addr_a, right_addr_b;
};

class GPU
{
    private:
        uint8_t* vram;

        FrameBuffer fb0, fb1;

        uint32_t read32_fb(FrameBuffer& fb, uint32_t addr);
        void write32_fb(FrameBuffer& fb, uint32_t addr, uint32_t value);
    public:
        GPU();
        ~GPU();

        void reset();

        uint32_t read_vram32(uint32_t addr);
        void write_vram32(uint32_t addr, uint32_t value);

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);

        uint8_t* get_buffer();
};

#endif // GPU_HPP
