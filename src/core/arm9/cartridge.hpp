#ifndef CTRCARD_HPP
#define CTRCARD_HPP
#include <cstdint>
#include <fstream>

struct NTR_ROMCTRL
{
    bool data_ready;
    bool busy;
};

struct CTR_ROMCTRL
{
    bool data_ready;
    bool write_mode;
    bool irq_enable;
    bool busy;
};

class DMA9;
class Interrupt9;

enum class SPICARD_STATE
{
    IDLE,
    SELECTED,
    NEEDS_PARAMS,
    WRITE_READY,
    PROGRAM_READY
};

class Cartridge
{
    private:
        DMA9* dma9;
        Interrupt9* int9;
        std::ifstream card;

        uint16_t ntr_enable;
        NTR_ROMCTRL ntr_romctrl;

        CTR_ROMCTRL ctr_romctrl;
        uint32_t ctr_secctrl;

        uint32_t cart_id;
        uint8_t cmd_buffer[16];

        uint8_t output_buffer[0x2000];
        int output_pos;
        int output_bytes_left;

        uint32_t read_addr;
        uint32_t read_block_count;

        uint8_t* save_data;
        uint32_t spi_save_addr;
        SPICARD_STATE spi_state;
        uint8_t spi_input_buffer[0x400];
        int spi_input_pos;
        uint8_t spi_output_buffer[0x400];
        int spi_output_pos;
        uint8_t spi_cmd;
        int spi_block_len;

        void process_ntr_cmd();
        void process_ctr_cmd();
        void process_spicard_cmd();
    public:
        Cartridge(DMA9* dma9, Interrupt9* int9);
        ~Cartridge();

        void reset();

        bool mount(std::string file_name);

        bool card_inserted();

        uint16_t read16_ntr(uint32_t addr);
        uint32_t read32_ntr(uint32_t addr);
        uint32_t read32_ctr(uint32_t addr);
        uint32_t read32_spicard(uint32_t addr);

        void write8_ntr(uint32_t addr, uint8_t value);
        void write16_ntr(uint32_t addr, uint16_t value);
        void write32_ntr(uint32_t addr, uint32_t value);
        void write8_ctr(uint32_t addr, uint8_t value);
        void write32_ctr(uint32_t addr, uint32_t value);
        void write32_spicard(uint32_t addr, uint32_t value);
};

#endif // CTRCARD_HPP
