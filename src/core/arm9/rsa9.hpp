#ifndef RSA_HPP
#define RSA_HPP
#include <cstdint>
#include <string>

struct RSA_CNT_REG
{
    uint8_t keyslot;
    bool big_endian;
    bool word_order;
};

struct RSA_KeySlot
{
    uint8_t exp[0x100];
    uint8_t mod[0x100];

    bool key_set;
    bool write_protect;
    int exp_ctr, mod_ctr;
};

class RSA
{
    private:
        RSA_CNT_REG RSA_CNT;
        RSA_KeySlot keys[4];

        uint8_t msg[0x100];
        int msg_ctr;

        void do_rsa_op();
    public:
        RSA();

        void reset();

        uint8_t read8(uint32_t addr);
        uint32_t read32(uint32_t addr);
        void write8(uint32_t addr, uint8_t value);
        void write32(uint32_t addr, uint32_t value);
};

#endif // RSA_HPP
