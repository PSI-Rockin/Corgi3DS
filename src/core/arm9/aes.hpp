#ifndef AES_HPP
#define AES_HPP
#include <cstdint>
#include <queue>
#include "aes_lib.hpp"

struct AES_CNT_REG
{
    uint8_t dma_write_size;
    uint8_t dma_read_size;
    uint8_t mac_size;
    bool mac_input_ctrl;
    bool mac_status;
    bool out_big_endian;
    bool in_big_endian;
    bool out_word_order;
    bool in_word_order;
    uint8_t mode;
    bool irq_enable;
    bool busy;
};

struct AES_KeySlot
{
    uint8_t normal[16];
    uint8_t x[16];
    uint8_t y[16];
};

class DMA9;
class Interrupt9;

class AES
{
    private:
        DMA9* dma9;
        Interrupt9* int9;
        AES_CNT_REG AES_CNT;
        uint8_t KEYSEL;
        uint8_t KEYCNT;

        uint16_t block_count, mac_count;

        AES_KeySlot keys[0x40];
        AES_KeySlot* cur_key;

        uint8_t AES_CTR[16];

        AES_ctx lib_aes_ctx;

        uint8_t crypt_results[16];

        uint8_t normal_fifo[16];
        uint8_t x_fifo[16];
        uint8_t y_fifo[16];
        int normal_ctr, x_ctr, y_ctr;

        uint8_t temp_input_fifo[16];
        int temp_input_ctr;
        std::queue<uint32_t> input_fifo, output_fifo;

        uint32_t most_recent_output;

        void init_aes_key(int slot);

        void gen_normal_key(int slot);
        void crypt_check();
        void input_vector(uint8_t* vector, int index, uint32_t value, int max_words);

        void crypt_ctr();
        void decrypt_cbc();
        void encrypt_cbc();
        void decrypt_ecb();

        void send_dma_requests();
    public:
        AES(DMA9* dma9, Interrupt9* int9);

        void reset();
        void write_input_fifo(uint32_t value);

        uint8_t read_keycnt();
        uint32_t read32(uint32_t addr);

        void write_block_count(uint16_t value);
        void write_keysel(uint8_t value);
        void write_keycnt(uint8_t value);
        void write32(uint32_t addr, uint32_t value);
};

#endif // AES_HPP
