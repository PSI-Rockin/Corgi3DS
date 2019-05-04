#ifndef SHA_HPP
#define SHA_HPP
#include <cstdint>
#include <queue>

struct SHA_CNT_REG
{
    bool busy;
    bool final_round;
    bool irq0_enable;
    bool out_big_endian;
    uint8_t mode;
    bool irq1_enable;
};

class SHA
{
    private:
        SHA_CNT_REG SHA_CNT;

        uint32_t hash[8];

        uint32_t messages[64];
        uint64_t message_len;

        std::queue<uint32_t> in_fifo;

        void reset_hash();

        void write_fifo(uint32_t value);
        void do_hash(bool final_round);
        void do_sha256(bool final_round);
        void _sha256();
    public:
        SHA();

        void reset();

        uint8_t read_hash(uint32_t addr);
        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // SHA_HPP
