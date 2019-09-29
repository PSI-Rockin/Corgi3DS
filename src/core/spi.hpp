#ifndef SPI_HPP
#define SPI_HPP
#include <cstdint>

struct NSPI_CNT
{
    uint8_t bandwidth;
    uint8_t device;
    bool busy;
};

class SPI
{
    private:
        int get_bus(uint32_t addr);

        NSPI_CNT nspi_cnt[3];
        int nspi_buff_pos[3];
        uint32_t nspi_len[3];
        uint32_t nspi_buff[3][0x100];
        uint32_t nspi_cmd[3];

        uint16_t touchscreen_x, touchscreen_y;

        void process_cmd(int bus);
    public:
        SPI();

        void reset();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);

        void set_touchscreen(uint16_t x, uint16_t y);
        void clear_touchscreen();
};

#endif // SPI_HPP
