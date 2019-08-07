#ifndef WIFI_HPP
#define WIFI_HPP
#include <cstdint>
#include <functional>
#include <queue>

struct SDIO_DATA32_IRQ
{
    bool data32_mode;
    bool tx32rq_irq_enable;
    bool rx32rdy_irq_enable;
};

struct WifiBlockTransfer
{
    uint32_t addr;
    uint8_t func;
    bool block_mode;
    uint16_t count;
    bool inc_addr;
    bool is_write;
    bool active;
};

class Corelink_DMA;

class WiFi
{
    private:
        Corelink_DMA* cdma;

        //SDIO registers
        std::function<void()> send_sdio_interrupt;
        uint32_t istat, imask;
        uint32_t argument;
        uint32_t response[4];
        uint16_t block16_len;
        SDIO_DATA32_IRQ data32_irq;
        WifiBlockTransfer block;
        bool card_irq_stat, card_irq_mask, old_card_irq;

        //WiFi registers
        uint32_t window_data;
        uint32_t window_read_addr;
        uint32_t window_write_addr;
        uint8_t eeprom[0x400];
        uint8_t mac[0x6];
        bool eeprom_ready;
        bool bmi_done;

        uint8_t irq_f0_stat;
        uint8_t irq_f0_mask;

        uint8_t irq_f1_stat;
        uint8_t irq_f1_mask;

        //FIFOs in F1 used to send BMI/WMI commands and receive replies to and from the card
        //Although four exist on real hardware, we use eight to have separate read/write FIFOs
        std::queue<uint8_t> mbox[8];

        void do_sdio_cmd(uint8_t cmd);

        void sdio_io_direct();
        uint8_t sdio_read_io(uint8_t func, uint32_t addr);
        uint8_t sdio_read_f0(uint32_t addr);
        uint8_t sdio_read_f1(uint32_t addr);
        void sdio_write_io(uint8_t func, uint32_t addr, uint8_t value);
        void sdio_write_f0(uint32_t addr, uint8_t value);
        void sdio_write_f1(uint32_t addr, uint8_t value);

        void sdio_io_extended();

        void command_end();
        void read_ready();
        void write_ready();
        void transfer_end();
        void set_istat(uint32_t value);

        void do_wifi_cmd();
        void do_bmi_cmd();
        void do_wmi_cmd();

        void send_wmi_reply(uint8_t* reply, uint32_t len, uint8_t eid, uint8_t flag, uint16_t ctrl);

        void check_card_irq();
        void check_f0_irq();
        void check_f1_irq();

        uint16_t read_fifo16();
        void write_fifo16(uint16_t value);

        uint32_t read_window(uint32_t addr);
        void write_window(uint32_t addr, uint32_t value);
    public:
        WiFi(Corelink_DMA* cdma);

        void reset();
        void set_sdio_interrupt_handler(std::function<void()> func);

        uint16_t read16(uint32_t addr);
        void write16(uint32_t addr, uint16_t value);

        uint32_t read_fifo32();
        void write_fifo32(uint32_t value);
};

#endif // WIFI_HPP
