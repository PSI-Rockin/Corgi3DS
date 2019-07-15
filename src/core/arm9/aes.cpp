#include <cstdio>
#include <cstring>
#include "aes.hpp"
#include "dma9.hpp"
#include "interrupt9.hpp"
#include "../common/common.hpp"

const static uint8_t key_const[] = {0x1F, 0xF9, 0xE9, 0xAA, 0xC5, 0xFE, 0x04, 0x08, 0x02, 0x45,
                                     0x91, 0xDC, 0x5D, 0x52, 0x76, 0x8A};

const static uint8_t dsi_const[] = {0xFF, 0xFE, 0xFB, 0x4E, 0x29, 0x59, 0x02, 0x58, 0x2A, 0x68,
                                     0x0F, 0x5F, 0x1A, 0x4F, 0x3E, 0x79};

//128-bit arithmetic functions used for generating a normal key
//Source: https://github.com/yellows8/3dscrypto-tools/blob/master/ctr-cryptotool/ctr-cryptotool.c
void n128_lrot(unsigned char *num, unsigned long shift)
{
    unsigned long tmpshift;
    unsigned int i;
    unsigned char tmp[16];

    while(shift)
    {
        tmpshift = shift;
        if(tmpshift>=8)tmpshift = 8;

        if(tmpshift==8)
        {
            for(i=0; i<16; i++)tmp[i] = num[i == 15 ? 0 : i+1];

            memcpy(num, tmp, 16);
        }
        else
        {
            for(i=0; i<16; i++)tmp[i] = (num[i] << tmpshift) | (num[i == 15 ? 0 : i+1] >> (8-tmpshift));
            memcpy(num, tmp, 16);
        }

        shift-=tmpshift;
    }
}

void n128_rrot(unsigned char *num, unsigned long shift)
{
    unsigned long tmpshift;
    unsigned int i;
    unsigned char tmp[16];

    while(shift)
    {
        tmpshift = shift;
        if(tmpshift>=8)tmpshift = 8;

        if(tmpshift==8)
        {
            for(i=0; i<16; i++)tmp[i] = num[i == 0 ? 15 : i-1];

            memcpy(num, tmp, 16);
        }
        else
        {
            for(i=0; i<16; i++)tmp[i] = (num[i] >> tmpshift) | (num[i == 0 ? 15 : i-1] << (8-tmpshift));
            memcpy(num, tmp, 16);
        }

        shift-=tmpshift;
    }
}

void n128_add(unsigned char *a, unsigned char *b)
{
    unsigned int i, carry=0, val;
    unsigned char tmp[16];
    unsigned char tmp2[16];
    unsigned char *out = (unsigned char*)a;

    memcpy(tmp, (unsigned char*)a, 16);
    memcpy(tmp2, (unsigned char*)b, 16);

    for(i=0; i<16; i++)
    {
        val = tmp[15-i] + tmp2[15-i] + carry;
        out[15-i] = (unsigned char)val;
        carry = val >> 8;
    }
}

AES::AES(DMA9* dma9, Interrupt9* int9) : dma9(dma9), int9(int9)
{

}

void AES::reset()
{
    KEYSEL = 0;
    KEYCNT = 0;
    cur_key = nullptr;
    normal_ctr = 0;
    temp_input_ctr = 0;
    x_ctr = 0;
    y_ctr = 0;

    AES_init_ctx(&lib_aes_ctx, (uint8_t*)keys[0x3F].normal);
}

void AES::gen_normal_key(int slot)
{
    uint8_t normal[16];
    memcpy(normal, keys[slot].x, 16);

    n128_lrot((uint8_t*)normal, 2);

    for (int i = 0; i < 16; i++)
        normal[i] ^= keys[slot].y[i];

    n128_add((uint8_t*)normal, (uint8_t*)key_const);
    n128_rrot((uint8_t*)normal, 41);

    printf("[AES] Generated key: ");
    for (int i = 0; i < 16; i++)
        printf("%02x", normal[i]);
    printf("\n");

    memcpy(keys[slot].normal, normal, 16);
}

void AES::gen_dsi_key(int slot)
{
    uint8_t normal[16];
    memcpy(normal, keys[slot].x, 16);

    for (int i = 0; i < 16; i++)
        normal[i] ^= keys[slot].y[i];

    n128_add((uint8_t*)normal, (uint8_t*)dsi_const);
    n128_lrot((uint8_t*)normal, 42);

    printf("[AES] Generated DSi key: ");
    for (int i = 0; i < 16; i++)
        printf("%02x", normal[i]);
    printf("\n");

    memcpy(keys[slot].normal, normal, 16);
}

void AES::init_aes_key(int slot)
{
    cur_key = &keys[slot];

    AES_init_ctx(&lib_aes_ctx, (uint8_t*)cur_key->normal);
}

void AES::crypt_check()
{
    if (input_fifo.size() >= 4 && output_fifo.size() <= 12 && AES_CNT.busy)
    {
        switch (AES_CNT.mode)
        {
            case 0x0:
                decrypt_ccm();
                break;
            case 0x2:
            case 0x3:
                crypt_ctr();
                break;
            case 0x4:
                decrypt_cbc();
                break;
            case 0x5:
                encrypt_cbc();
                break;
            case 0x6:
                decrypt_ecb();
                break;
            default:
                EmuException::die("[AES] Unrecognized crypt mode %d\n", AES_CNT.mode);
        }

        //Copy results into output FIFO
        for (int i = 0; i < 4; i++)
        {
            int index = i << 2;
            if (!AES_CNT.out_word_order)
            {
                index = 12 - index;
            }

            uint32_t value = *(uint32_t*)&crypt_results[index];
            if (!AES_CNT.out_big_endian)
                value = bswp32(value);

            printf("Output FIFO: $%08X\n", value);
            output_fifo.push(value);
        }

        block_count--;

        //printf("Blocks left: %d\n", block_count);

        if (!block_count)
        {
            AES_CNT.busy = false;
            if (AES_CNT.irq_enable)
                int9->assert_irq(15);
        }
    }
    send_dma_requests();
}

void AES::write_input_fifo(uint32_t value)
{
    input_vector((uint8_t*)temp_input_fifo, temp_input_ctr, value, 4);
    temp_input_ctr++;
    if (temp_input_ctr == 4)
    {
        temp_input_ctr = 0;
        for (int i = 0; i < 4; i++)
        {
            input_fifo.push(*(uint32_t*)&temp_input_fifo[i * 4]);
            //printf("Input FIFO: $%08X\n", *(uint32_t*)&temp_input_fifo[i * 4]);
        }
    }

    crypt_check();
}

void AES::decrypt_ccm()
{
    printf("[AES] Decrypt CCM\n");

    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_CNT.mac_status = true;
}

void AES::crypt_ctr()
{
    printf("[AES] Crypt CTR\n");

    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_CTR_xcrypt_buffer(&lib_aes_ctx, (uint8_t*)crypt_results, 16);
}

void AES::decrypt_cbc()
{
    printf("[AES] Decrypt CBC\n");
    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_CBC_decrypt_buffer(&lib_aes_ctx, (uint8_t*)crypt_results, 16);
}

void AES::encrypt_cbc()
{
    printf("[AES] Encrypt CBC\n");
    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_CBC_encrypt_buffer(&lib_aes_ctx, (uint8_t*)crypt_results, 16);
}

void AES::decrypt_ecb()
{
    printf("[AES] Decrypt ECB\n");
    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_ECB_decrypt(&lib_aes_ctx, (uint8_t*)crypt_results);
}

uint8_t AES::read_keycnt()
{
    return KEYCNT;
}

uint32_t AES::read32(uint32_t addr)
{
    uint32_t reg = 0;
    switch (addr)
    {
        case 0x10009000:
            reg |= input_fifo.size();
            reg |= output_fifo.size() << 5;
            reg |= AES_CNT.dma_write_size << 12;
            reg |= AES_CNT.dma_read_size << 14;
            reg |= AES_CNT.mac_size << 16;
            reg |= AES_CNT.mac_input_ctrl << 20;
            reg |= AES_CNT.mac_status << 21;
            reg |= AES_CNT.out_big_endian << 22;
            reg |= AES_CNT.in_big_endian << 23;
            reg |= AES_CNT.out_word_order << 24;
            reg |= AES_CNT.in_word_order << 25;
            reg |= AES_CNT.mode << 27;
            reg |= AES_CNT.irq_enable << 30;
            reg |= AES_CNT.busy << 31;
            printf("[AES] Read CNT: $%08X\n", reg);
            break;
        case 0x1000900C:
            if (output_fifo.size())
            {
                most_recent_output = output_fifo.front();
                output_fifo.pop();
            }
            reg = most_recent_output;
            crypt_check();
            //printf("[AES] Read RDFIFO: $%08X\n", reg);
            break;
        default:
            printf("[AES] Unrecognized read32 $%08X\n", addr);
    }
    return reg;
}

void AES::write_mac_count(uint16_t value)
{
    printf("[AES] MAC count: $%04X\n", value);
    mac_count = value;
}

void AES::write_block_count(uint16_t value)
{
    printf("[AES] Block count: $%04X\n", value);
    block_count = value;
}

void AES::write_keysel(uint8_t value)
{
    printf("[AES] KEYSEL: $%02X\n", value);
    KEYSEL = value;
}

void AES::write_keycnt(uint8_t value)
{
    printf("[AES] KEYCNT: $%02X\n", value);
    KEYCNT = value;
}

void AES::write32(uint32_t addr, uint32_t value)
{
    if (addr >= 0x10009020 && addr < 0x10009030)
    {
        printf("[AES] Write CTR $%08X: $%08X\n", addr, value);

        input_vector((uint8_t*)AES_CTR, 3 - ((addr / 4) & 0x3), value, 4, true);
        AES_ctx_set_iv(&lib_aes_ctx, (uint8_t*)AES_CTR);
        return;
    }

    //KEY0/1/2/3 - mirrors of the DSi key registers
    if (addr >= 0x10009040 && addr < 0x10009100)
    {
        addr -= 0x10009040;
        int key = (addr / 48) & 0x3;
        int fifo_id = (addr / 16) % 3;
        int offset = 3 - ((addr / 4) & 0x3);

        switch (fifo_id)
        {
            case 0:
                printf("[AES] Write DSi KEY%d NORMAL: $%08X\n", key, value);
                input_vector((uint8_t*)keys[key].normal, offset, value, 4, true);
                break;
            case 1:
                printf("[AES] Write DSi KEY%d X: $%08X\n", key, value);
                input_vector((uint8_t*)keys[key].x, offset, value, 4, true);
                break;
            case 2:
                printf("[AES] Write DSi KEY%d Y: $%08X\n", key, value);
                input_vector((uint8_t*)keys[key].y, offset, value, 4, true);

                //Keygen is done every time the keyslot is updated
                gen_dsi_key(key);
                break;
        }
        printf("Addr: $%08X\n", addr + 0x10009040);
        return;
    }

    switch (addr)
    {
        case 0x10009000:
            printf("[AES] Write CNT: $%08X\n", value);
            if ((AES_CNT.in_word_order << 25) ^ (value & (1 << 25)))
            {
                //Flush key FIFOs
                normal_ctr = 0;
                x_ctr = 0;
                y_ctr = 0;
            }
            AES_CNT.dma_write_size = (value >> 12) & 0x3;
            AES_CNT.dma_read_size = (value >> 14) & 0x3;
            AES_CNT.mac_size = (value >> 16) & 0x7;
            AES_CNT.mac_input_ctrl = value & (1 << 20);
            AES_CNT.out_big_endian = value & (1 << 22);
            AES_CNT.in_big_endian = value & (1 << 23);
            AES_CNT.out_word_order = value & (1 << 24);
            AES_CNT.in_word_order = value & (1 << 25);
            AES_CNT.mode = (value >> 27) & 0x7;
            AES_CNT.irq_enable = value & (1 << 30);
            AES_CNT.busy = value & (1 << 31);

            send_dma_requests();
            dma9->clear_ndma_req(NDMA_AES2);

            if (value & (1 << 26))
            {
                cur_key = &keys[KEYSEL & 0x3F];
                init_aes_key(KEYSEL & 0x3F);
            }
            return;
        case 0x10009004:
            mac_count = value & 0xFFFF;
            block_count = (value >> 16);
            printf("[AES] MAC count: $%08X\n", mac_count);
            return;
        case 0x10009008:
            //printf("[AES] Write WRFIFO: $%08X\n", value);
            write_input_fifo(value);
            return;
        case 0x10009100:
            printf("[AES] Write KEYFIFO: $%08X\n", value);
            input_vector((uint8_t*)normal_fifo, normal_ctr, value, 4);
            normal_ctr++;

            //If key fifo is full, copy it over to current keyslot
            if (normal_ctr >= 4)
            {
                normal_ctr = 0;
                memcpy(keys[KEYCNT & 0x3F].normal, normal_fifo, 16);
            }
            return;
        case 0x10009104:
            printf("[AES] Write XFIFO: $%08X\n", value);
            input_vector((uint8_t*)x_fifo, x_ctr, value, 4);
            x_ctr++;

            if (x_ctr >= 4)
            {
                x_ctr = 0;
                memcpy(keys[KEYCNT & 0x3F].x, x_fifo, 16);
            }
            return;
        case 0x10009108:
            printf("[AES] Write YFIFO: $%08X\n", value);
            input_vector((uint8_t*)y_fifo, y_ctr, value, 4);
            y_ctr++;

            //When the y fifo is filled, the x and y keys are used to generate a normal key.
            if (y_ctr >= 4)
            {
                y_ctr = 0;
                memcpy(keys[KEYCNT & 0x3F].y, y_fifo, 16);
                gen_normal_key(KEYCNT & 0x3F);
            }
            return;
    }
    printf("[AES] Unrecognized write32 $%08X: $%08X\n", addr, value);
}

void AES::input_vector(uint8_t *vector, int index, uint32_t value, int max_words, bool force_order)
{
    //Reverse word order
    if (!AES_CNT.in_word_order && !force_order)
        index = (max_words - 1) - index;

    if (!AES_CNT.in_big_endian)
        value = bswp32(value);

    index <<= 2;
    vector[index] = value & 0xFF;
    vector[index + 1] = (value >> 8) & 0xFF;
    vector[index + 2] = (value >> 16) & 0xFF;
    vector[index + 3] = value >> 24;
}

void AES::send_dma_requests()
{
    if (!AES_CNT.busy)
    {
        dma9->clear_ndma_req(NDMA_AES_WRITEFREE);
    }
    else
    {
        if (input_fifo.size() <= 8)
            dma9->set_ndma_req(NDMA_AES_WRITEFREE);
        else
            dma9->clear_ndma_req(NDMA_AES_WRITEFREE);
    }

    if (output_fifo.size() >= 4)
        dma9->set_ndma_req(NDMA_AES_READFREE);
    else
        dma9->clear_ndma_req(NDMA_AES_READFREE);
}
