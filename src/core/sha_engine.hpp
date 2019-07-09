#ifndef SHA_ENGINE_HPP
#define SHA_ENGINE_HPP
#include <queue>

struct SHA_CNT_REG
{
    bool busy;
    bool final_round;
    bool in_dma_enable;
    bool out_big_endian;
    uint8_t mode;
    bool fifo_enable;
    bool out_dma_enable;
};

struct SHA_Engine
{
    SHA_CNT_REG SHA_CNT;

    uint32_t hash[8];

    uint32_t messages[80];
    uint64_t message_len;

    std::queue<uint32_t> in_fifo;
    std::queue<uint32_t> read_fifo;

    void reset_hash();

    void do_hash(bool final_round);
    void do_sha256(bool final_round);
    void do_sha1(bool final_round);
    void _sha256();
    void _sha1();
};

#endif // SHA_ENGINE_HPP
