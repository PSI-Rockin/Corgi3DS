#include <cstdio>
#include <cstring>
#include <sstream>
#include "../common/common.hpp"
#include "interrupt9.hpp"
#include "rsa.hpp"

RSA::RSA(Interrupt9* int9) : int9(int9)
{

}

void RSA::reset()
{
    memset(keys, 0, sizeof(keys));
}

uint8_t RSA::read8(uint32_t addr)
{
    if (addr >= 0x1000B800 && addr < 0x1000B900)
    {
        int index = addr & 0xFF;
        if (!RSA_CNT.word_order)
            index = 0xFF - index;
        return msg[index];
    }
    printf("[RSA] Unrecognized read8 $%08X\n", addr);
    return 0;
}

uint32_t RSA::read32(uint32_t addr)
{
    uint32_t reg = 0;
    if (addr >= 0x1000B100 && addr < 0x1000B140)
    {
        int index = (addr / 0x10) & 0x3;
        int reg = (addr / 4) & 0x3;

        switch (reg)
        {
            case 0:
                //reg |= keys[index].key_set;
                reg = 1;
                reg |= keys[index].write_protect << 1;
                printf("[RSA] Read key%d cnt\n", index);
                return reg;
            case 1:
                printf("[RSA] Read key%d size\n", index);
                return 0x40;
        }
    }

    if (addr >= 0x1000B800 && addr < 0x1000B900)
    {
        int index = addr & 0xFF;
        if (!RSA_CNT.word_order)
            index = 0xFC - index;
        uint32_t value = *(uint32_t*)&msg[index];
        if (!RSA_CNT.big_endian)
            value = bswp32(value);
        printf("[RSA] Read TXT $%08X: $%08X\n", addr, value);
        return value;
    }

    switch (addr)
    {
        case 0x1000B000:
            reg |= RSA_CNT.keyslot << 4;
            reg |= RSA_CNT.big_endian << 8;
            reg |= RSA_CNT.word_order << 9;
            printf("[RSA] Read CNT: $%08X\n", reg);
            break;
        default:
            printf("[RSA] Unrecognized read32 $%08X\n", addr);
            return 0;
    }
    return reg;
}

void RSA::write8(uint32_t addr, uint8_t value)
{
    if (addr >= 0x1000B200 && addr < 0x1000B300)
    {
        printf("[RSA] Write8 key%d exp: $%02X\n", RSA_CNT.keyslot, value);
        RSA_KeySlot* key = &keys[RSA_CNT.keyslot];
        //if (RSA_CNT.big_endian)
            //value = bswp32(value);

        key->exp[key->exp_ctr] = value;
        key->exp_ctr++;
        if (key->exp_ctr >= 0x100)
        {
            key->exp_ctr = 0;
        }
        return;
    }

    if (addr >= 0x1000B400 && addr < 0x1000B500)
    {
        printf("[RSA] Write8 key%d mod: $%02X\n", RSA_CNT.keyslot, value);
        RSA_KeySlot* key = &keys[RSA_CNT.keyslot];
        //if (RSA_CNT.big_endian)
            //value = bswp32(value);

        int index = key->mod_ctr;
        if (!RSA_CNT.word_order)
            index = 0xFF - index;

        key->mod[index] = value;
        key->mod_ctr++;
        if (key->mod_ctr >= 0x100)
        {
            key->mod_ctr = 0;
        }
        return;
    }

    if (addr >= 0x1000B800 && addr < 0x1000B900)
    {
        printf("[RSA] Write TXT: $%02X (%d)\n", value, msg_ctr);
        if (!RSA_CNT.word_order)
            msg[0xFF - msg_ctr] = value;
        else
            msg[msg_ctr] = value;

        msg_ctr++;
        if (msg_ctr >= 0x100)
            msg_ctr = 0;
        return;
    }

    printf("[RSA] Unrecognized write8 $%08X\n", addr);
}

void RSA::write32(uint32_t addr, uint32_t value)
{
    if (addr >= 0x1000B100 && addr < 0x1000B140)
    {
        int index = (addr / 0x10) & 0x3;
        int reg = (addr / 4) & 0x3;

        switch (reg)
        {
            case 0:
                printf("[RSA] Write32 key%d cnt: $%08X\n", index, value);
                keys[index].write_protect = value & (1 << 1);
                return;
        }
    }

    if (addr >= 0x1000B200 && addr < 0x1000B300)
    {
        printf("[RSA] Write32 key%d exp: $%08X\n", RSA_CNT.keyslot, value);
        RSA_KeySlot* key = &keys[RSA_CNT.keyslot];
        if (!RSA_CNT.big_endian)
            value = bswp32(value);

        *(uint32_t*)&key->exp[key->exp_ctr] = value;
        key->exp_ctr += 4;
        if (key->exp_ctr >= 0x100)
        {
            key->exp_ctr = 0;
        }
        return;
    }

    if (addr >= 0x1000B400 && addr < 0x1000B500)
    {
        printf("[RSA] Write32 key%d mod: $%08X\n", RSA_CNT.keyslot, value);
        RSA_KeySlot* key = &keys[RSA_CNT.keyslot];
        if (!RSA_CNT.big_endian)
            value = bswp32(value);

        int index = key->mod_ctr;
        if (!RSA_CNT.word_order)
            index = 0xFC - index;

        *(uint32_t*)&key->mod[index] = value;
        key->mod_ctr += 4;
        if (key->mod_ctr >= 0x100)
        {
            key->mod_ctr = 0;
        }
        return;
    }

    if (addr >= 0x1000B800 && addr < 0x1000B900)
    {
        printf("[RSA] Write TXT: $%08X (%d)\n", value, msg_ctr);
        if (!RSA_CNT.big_endian)
            value = bswp32(value);
        if (!RSA_CNT.word_order)
            *(uint32_t*)&msg[0xFC - msg_ctr] = value;
        else
            *(uint32_t*)&msg[msg_ctr] = value;

        msg_ctr += 4;
        if (msg_ctr >= 0x100)
            msg_ctr = 0;
        return;
    }

    switch (addr)
    {
        case 0x1000B000:
            printf("[RSA] Write CNT: $%08X\n", value);
            RSA_CNT.keyslot = (value >> 4) & 0x3;
            RSA_CNT.big_endian = value & (1 << 8);
            RSA_CNT.word_order = value & (1 << 9);

            if (value & 0x1)
                do_rsa_op();
            break;
        default:
            printf("[RSA] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}

void RSA::convert_to_bignum(uint8_t *src, mpz_t dest)
{
    mpz_t base, temp;

    mpz_init(temp);
    mpz_init_set_ui(dest, 0);
    mpz_init_set_ui(base, 1);
    for (int i = 0xFF; i >= 0; i--)
    {
        mpz_set_ui(temp, src[i]);
        mpz_mul(temp, temp, base);
        mpz_add(dest, dest, temp);
        mpz_mul_ui(base, base, 256);
    }
}

void RSA::do_rsa_op()
{
    mpz_t gmp_msg, gmp_b, gmp_e, gmp_m;

    RSA_KeySlot* key = &keys[RSA_CNT.keyslot];
    mpz_inits(gmp_msg, NULL);
    convert_to_bignum((uint8_t*)msg, gmp_b);
    convert_to_bignum((uint8_t*)key->exp, gmp_e);
    convert_to_bignum((uint8_t*)key->mod, gmp_m);

    printf("Msg: %s\n", mpz_get_str(NULL, 16, gmp_b));
    printf("Exp: %s\n", mpz_get_str(NULL, 16, gmp_e));
    printf("Mod: %s\n", mpz_get_str(NULL, 16, gmp_m));

    mpz_powm(gmp_msg, gmp_b, gmp_e, gmp_m);
    printf("Result: %s\n", mpz_get_str(NULL, 16, gmp_msg));

    convert_from_bignum(gmp_msg, msg);
    int9->assert_irq(22);
}

void RSA::convert_from_bignum(mpz_t src, uint8_t* dest)
{
    std::string str = mpz_get_str(NULL, 16, src);

    while (str.length() < 0x200)
        str = '0' + str;

    for (int i = 0; i < 0x100; i++)
    {
        uint8_t chr1 = str[(i * 2)];
        uint8_t chr2 = str[(i * 2) + 1];

        if (chr1 >= 'a')
            chr1 = (chr1 - 'a') + 10;
        else
            chr1 -= '0';

        if (chr2 >= 'a')
            chr2 = (chr2 - 'a') + 10;
        else
            chr2 -= '0';

        uint8_t byte = (chr1 << 4) | chr2;

        dest[i] = byte;
    }
}
