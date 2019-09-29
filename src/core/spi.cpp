#include <cstdio>
#include <cstring>
#include "spi.hpp"

#include "common/common.hpp"

SPI::SPI()
{

}

int SPI::get_bus(uint32_t addr)
{
    uint32_t bus_addr = addr & 0xFFFFFF00;
    if (bus_addr == 0x10142800)
        return 0;
    if (bus_addr == 0x10143000)
        return 1;
    return 2;
}

void SPI::reset()
{
    memset(nspi_cnt, 0, sizeof(nspi_cnt));
    memset(nspi_buff_pos, 0, sizeof(nspi_buff_pos));
    memset(nspi_cmd, 0, sizeof(nspi_cmd));

    clear_touchscreen();
}

uint32_t SPI::read32(uint32_t addr)
{
    int bus = get_bus(addr);
    int reg = (addr & 0xFF) >> 2;
    switch (reg)
    {
        case 0x00:
            printf("[SPI] Read NSPI_CNT%d\n", bus);
            return 0;
        case 0x03:
        {
            printf("[SPI] Read NSPI_FIFO%d\n", bus);

            uint32_t value = nspi_buff[bus][nspi_buff_pos[bus]];
            nspi_buff_pos[bus]++;
            nspi_len[bus] -= 4;

            if (!nspi_len[bus])
                nspi_buff_pos[bus] = 0;

            printf("Value: $%08X\n", value);

            return value;
        }
        default:
            printf("[SPI] Unrecognized bus%d read32 $%08X\n", bus, addr);
            return 0;
    }
}

void SPI::write32(uint32_t addr, uint32_t value)
{
    int bus = get_bus(addr);
    int reg = (addr & 0xFF) >> 2;
    switch (reg)
    {
        case 0x00:
            printf("[SPI] Write NSPI_CNT%d: $%08X\n", bus, value);
            nspi_cnt[bus].bandwidth = value & 0x7;
            nspi_cnt[bus].device = (value >> 6) & 0x3;
            nspi_cnt[bus].busy = (value >> 15) & 0x1;
            break;
        case 0x01:
            printf("[SPI] Write NSPI_DONE%d: $%08X\n", bus, value);
            if (!(value & 0x1))
                nspi_cmd[bus] = 0;
            break;
        case 0x02:
            printf("[SPI] Write NSPI_LEN%d: $%08X\n", bus, value);
            nspi_len[bus] = value;
            break;
        case 0x03:
            printf("[SPI] Write NSPI_FIFO%d: $%08X\n", bus, value);
            if (!nspi_cmd[bus])
            {
                nspi_cmd[bus] = value;
                process_cmd(bus);
            }
            else
            {
                if (nspi_len[bus] >= 0x100)
                    EmuException::die("[SPI] Blocklen exceeds buffer size!");

                //nspi_buff[bus][nspi_buff_pos[bus]] = value;
                nspi_buff_pos[bus]++;

                nspi_len[bus] -= 4;
                if (!nspi_len[bus])
                {
                    nspi_buff_pos[bus] = 0;
                }
            }
            break;
        default:
            printf("[SPI] Unrecognized bus%d write32 $%08X: $%08X\n", bus, addr, value);
    }
}

void SPI::process_cmd(int bus)
{
    int device = nspi_cnt[bus].device | (bus << 4);

    memset(nspi_buff[bus], 0, sizeof(nspi_buff[bus]));
    nspi_buff_pos[bus] = 0;

    uint8_t temp_buff[0x34];

    switch (device)
    {
        //CODEC
        case 0x00:
            switch (nspi_cmd[bus])
            {
                case 0x03:
                    //Read input info

                    //Touchscreen
                    for (int i = 0; i < 5; i++)
                    {
                        *(uint16_t*)&temp_buff[i << 1] = touchscreen_x;
                        *(uint16_t*)&temp_buff[(i << 1) + 10] = touchscreen_y;
                    }

                    //Circle pad positions
                    for (int i = 0; i < 4; i++)
                    {
                        *(uint32_t*)&temp_buff[(i << 2) + 0x14] = 0x80008;
                        *(uint32_t*)&temp_buff[(i << 2) + 0x24] = 0x80008;
                    }

                    memcpy(nspi_buff[bus], temp_buff, 0x34);
                    break;
                default:
                    printf("[CODEC] Unrecognized command $%02X\n", nspi_cmd[bus]);
            }
            break;
        default:
            printf("[SPI] Unrecognized device $%02X\n", device);
    }
}

void SPI::set_touchscreen(uint16_t x, uint16_t y)
{
    //We need to scale from pixel coordinates (on a 320x240 screen) to touchscreen coordinates (4096x4096)
    touchscreen_x = x * 4096 / 320;
    touchscreen_y = y * 4096 / 240;

    touchscreen_x = bswp16(touchscreen_x);
    touchscreen_y = bswp16(touchscreen_y);

    printf("TOUCH! %d %d\n", touchscreen_x, touchscreen_y);
}

void SPI::clear_touchscreen()
{
    touchscreen_x = 0xFFFF;
    touchscreen_y = 0xFFFF;
}
