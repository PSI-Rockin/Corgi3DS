#ifndef SHA_HPP
#define SHA_HPP
#include <cstdint>
#include <queue>
#include "../sha_engine.hpp"

class DMA9;

class SHA
{
    private:
        DMA9* dma9;

        SHA_Engine eng;

        void write_fifo(uint32_t value);
    public:
        SHA(DMA9* dma9);

        void reset();

        uint8_t read_hash(uint32_t addr);
        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // SHA_HPP
