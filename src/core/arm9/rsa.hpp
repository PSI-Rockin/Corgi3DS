#ifndef RSA_HPP
#define RSA_HPP
#include <cstdint>
#include <string>

#include "../../external/mini-gmp.h"

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

class Interrupt9;

class RSA
{
    private:
        Interrupt9* int9;
        RSA_CNT_REG RSA_CNT;
        RSA_KeySlot keys[4];

        uint8_t msg[0x100];
        int msg_ctr;

        void do_rsa_op();
        void convert_to_bignum(uint8_t* src, mpz_t dest);
        void convert_from_bignum(mpz_t src, uint8_t* dest);
    public:
        RSA(Interrupt9* int9);

        void reset();

        uint8_t read8(uint32_t addr);
        uint32_t read32(uint32_t addr);
        void write8(uint32_t addr, uint8_t value);
        void write32(uint32_t addr, uint32_t value);
};

#endif // RSA_HPP
