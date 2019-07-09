#ifndef HASH_HPP
#define HASH_HPP
#include "../sha_engine.hpp"

class HASH
{
    private:
        SHA_Engine eng;

    public:
        HASH();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
        void write_fifo(uint32_t value);
};

#endif // HASH_HPP
