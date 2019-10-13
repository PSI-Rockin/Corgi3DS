#include <fstream>
#include <cstring>
#include "../common/common.hpp"
#include "../corelink_dma.hpp"
#include "../scheduler.hpp"
#include "wifi.hpp"

//#define LLE_WIFI

constexpr static int ROM_BASE = 0x0E0000;
constexpr static int RAM_BASE = 0x120000;
constexpr static int MEMMAP_MASK = (1024 * 1024 * 4) - 1;

const static uint8_t CIS0[256] =
{
    0x01, 0x03, 0xD9, 0x01, 0xFF,
    0x20, 0x04, 0x71, 0x02, 0x00, 0x02,
    0x21, 0x02, 0x0C, 0x00,
    0x22, 0x04, 0x00, 0x00, 0x08, 0x32,
    0x1A, 0x05, 0x01, 0x01, 0x00, 0x02, 0x07,
    0x1B, 0x08, 0xC1, 0x41, 0x30, 0x30, 0xFF, 0xFF, 0x32, 0x00,
    0x14, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};

const static uint8_t CIS1[256] =
{
    0x20, 0x04, 0x71, 0x02, 0x00, 0x02,
    0x21, 0x02, 0x0C, 0x00,
    0x22, 0x2A, 0x01,
    0x01, 0x11,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08,
    0x00, 0x00, 0xFF, 0x80,
    0x00, 0x00, 0x00,
    0x00, 0x01, 0x0A,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x01,
    0x80, 0x01, 0x06,
    0x81, 0x01, 0x07,
    0x82, 0x01, 0xDF,
    0xFF,
    0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

uint8_t read8_mbox(std::queue<uint8_t>& mbox)
{
    uint8_t value = mbox.front();
    mbox.pop();
    return value;
}

uint16_t read16_mbox(std::queue<uint8_t>& mbox)
{
    uint16_t value = 0;
    for (int i = 0; i < 2; i++)
    {
        value |= mbox.front() << (i * 8);
        mbox.pop();
    }
    return value;
}

uint32_t read32_mbox(std::queue<uint8_t>& mbox)
{
    uint32_t value = 0;
    for (int i = 0; i < 4; i++)
    {
        value |= mbox.front() << (i * 8);
        mbox.pop();
    }
    return value;
}

void write8_mbox(std::queue<uint8_t>& mbox, uint8_t value)
{
    mbox.push(value);
}

void write16_mbox(std::queue<uint8_t>& mbox, uint16_t value)
{
    for (int i = 0; i < 2; i++)
        mbox.push((value >> (i * 8)) & 0xFF);
}

void write32_mbox(std::queue<uint8_t>& mbox, uint32_t value)
{
    for (int i = 0; i < 4; i++)
        mbox.push((value >> (i * 8)) & 0xFF);
}

WiFi::WiFi(Corelink_DMA* cdma, Scheduler* scheduler) :
    cdma(cdma),
    scheduler(scheduler),
    xtensa(this)
{
    send_sdio_interrupt = nullptr;
    ROM = nullptr;
    RAM = nullptr;
}

WiFi::~WiFi()
{
    delete[] ROM;
    delete[] RAM;
}

void WiFi::reset()
{
    if (!ROM)
        ROM = new uint8_t[0x40000];
    if (!RAM)
        RAM = new uint8_t[0x20000];

#ifdef LLE_WIFI
    std::ifstream rom_dump("AR6014_ROM.bin");
    rom_dump.read((char*)ROM, 0x40000);
    rom_dump.close();
#else
    memset(ROM, 0, 0x40000);
#endif

    timers.reset();
    xtensa.reset();

    timers.set_send_soc_irq([this] (int id) {send_xtensa_soc_irq(id);});

    memset(RAM, 0, 0x20000);

    istat = 0;
    imask = 0;

    block.active = false;
    bmi_done = false;
    card_irq_mask = false;
    card_irq_stat = false;
    old_card_irq = false;

    for (int i = 0; i < 6; i++)
        mac[i] = i;

    //Reset EEPROM to 0
    memset(eeprom, 0, 0x400);

    *(uint32_t*)&eeprom[0x0] = 0x300;

    //0x8000+country code (in this case, USA)
    *(uint16_t*)&eeprom[0x8] = 0x8348;

    memcpy(eeprom + 0xA, mac, 6);

    *(uint32_t*)&eeprom[0x10] = 0x60000000;

    memset(eeprom + 0x3C, 0xFF, 0x70);
    memset(eeprom + 0x140, 0xFF, 8);

    //Checksum
    uint16_t checksum = 0xFFFF;
    for (int i = 0; i < 0x300; i += 2)
        checksum ^= *(uint16_t*)&eeprom[i];

    *(uint16_t*)&eeprom[0x4] = checksum;

    irq_f0_mask = 0;
    irq_f0_stat = 0;
    irq_f1_mask = 0;
    irq_f1_stat = 0;

    xtensa_irq_stat = 0;
    xtensa_mbox_irq_enable = 0;
    xtensa_mbox_irq_stat = 0;
}

void WiFi::run(int cycles)
{
#ifdef LLE_WIFI
    xtensa.run(cycles);
    timers.run(cycles);
#endif
}

void WiFi::do_sdio_cmd(uint8_t cmd)
{
    printf("[WiFi] CMD%d\n", cmd);
    printf("Arg: $%08X\n", argument);
    switch (cmd)
    {
        case 52:
            sdio_io_direct();
            command_end();
            break;
        case 53:
            sdio_io_extended();
            break;
        default:
            EmuException::die("[WiFi] Unrecognized SDIO cmd%d", cmd);
    }
}

void WiFi::sdio_io_direct()
{
    bool is_write = argument >> 31;
    uint8_t func = (argument >> 28) & 0x7;
    bool read_after_write = (argument >> 27) & 0x1;
    uint32_t addr = (argument >> 9) & 0x1FFFF;
    uint8_t data;

    printf("[WiFi] Single transfer - addr: %d:%05X\n", func, addr);
    printf("Is write: %d RAW: %d\n", is_write, read_after_write);

    if (is_write)
    {
        data = argument & 0xFF;
        sdio_write_io(func, addr, data);

        if (read_after_write)
            data = sdio_read_io(func, addr);
    }
    else
        data = sdio_read_io(func, addr);

    response[0] = data;
    response[0] |= 0x1000;
}

uint8_t WiFi::sdio_read_io(uint8_t func, uint32_t addr)
{
    switch (func)
    {
        case 0:
            return sdio_read_f0(addr);
        case 1:
            return sdio_read_f1(addr);
        default:
            printf("[WiFi] Unrecognized IO read %d:%05X\n", func, addr);
            return 0;
    }
}

uint8_t WiFi::sdio_read_f0(uint32_t addr)
{
    uint8_t value = 0;
    if (addr >= 0x01000 && addr < 0x01100)
        return CIS0[addr & 0xFF];
    if (addr >= 0x01100 && addr < 0x01200)
        return CIS1[addr & 0xFF];
    switch (addr)
    {
        case 0x00000:
            //Revision
            value = 0x11;
            break;
        case 0x00002:
            //Function enable
            value = 0x02;
            break;
        case 0x00003:
            //Function ready
            value = 0x02;
            break;
        case 0x00004:
            value = irq_f0_mask;
            break;
        case 0x00005:
            value = irq_f0_stat;
            break;
        case 0x00008:
            //Card capability
            value = 0x17;
            break;
        case 0x00009:
            //Common CIS pointer low
            value = 0;
            break;
        case 0x0000A:
            //...mid
            value = 0x10;
            break;
        case 0x0000B:
            //...high
            value = 0;
            break;
        case 0x00012:
            //Power control
            value = 0x3;
            break;
        case 0x00109:
            //CIS pointer low
            value = 0;
            break;
        case 0x0010A:
            //...mid
            value = 0x11;
            break;
        case 0x0010B:
            //...high
            value = 0;
            break;
        default:
            printf("[WiFi] Unrecognized F0 read $%05X\n", addr);
    }
    return value;
}

uint8_t WiFi::sdio_read_f1(uint32_t addr)
{
    uint8_t value = 0;
    if (addr < 0x100)
    {
        value = read8_mbox(mbox[4]);
        check_f1_irq();
        return value;
    }
    if (addr >= 0x800 && addr < 0x1000)
    {
        value = read8_mbox(mbox[4]);
        check_f1_irq();
        return value;
    }
    switch (addr)
    {
        case 0x00400:
            value = irq_f1_stat;
            printf("[WiFi] IRQ status: $%02X\n", value);
            break;
        case 0x00405:
            for (int i = 0; i < 4; i++)
                value |= (mbox[i + 4].size() >= 4) << i;
            printf("[WiFi] Mbox status: $%02X\n", value);
            break;
        case 0x00408:
        case 0x00409:
        case 0x0040A:
        case 0x0040B:
            if (mbox[4].size() >= 4)
                value = *(&mbox[4].front() + (addr - 0x00408));
            printf("[WiFi] Peek0: $%02X\n", value);
            break;
        case 0x00418:
            value = irq_f1_mask;
            break;
        case 0x00474:
            return window_data & 0xFF;
        case 0x00475:
            return (window_data >> 8) & 0xFF;
        case 0x00476:
            return (window_data >> 16) & 0xFF;
        case 0x00477:
            return window_data >> 24;
        default:
            printf("[WiFi] Unrecognized F1 read $%05X\n", addr);
    }
    return value;
}

void WiFi::sdio_write_io(uint8_t func, uint32_t addr, uint8_t value)
{
    switch (func)
    {
        case 0:
            sdio_write_f0(addr, value);
            break;
        case 1:
            sdio_write_f1(addr, value);
            break;
        default:
            printf("[WiFi] Unrecognized IO write %d:%05X: $%02X\n", func, addr, value);
    }
}

void WiFi::sdio_write_f0(uint32_t addr, uint8_t value)
{
    switch (addr)
    {
        case 0x00004:
            printf("[WiFi] Set F0 IRQ mask: $%02X\n", value);
            irq_f0_mask = value;
            check_f0_irq();
            break;
        default:
            printf("[WiFi] Unrecognized F0 write $%05X: $%02X\n", addr, value);
    }
}

void WiFi::sdio_write_f1(uint32_t addr, uint8_t value)
{
    if (addr < 0x100)
    {
        write8_mbox(mbox[0], value);
        if (addr == 0xFF)
            do_wifi_cmd();
        check_f1_irq();
        return;
    }
    if (addr >= 0x800 && addr < 0x1000)
    {
        write8_mbox(mbox[0], value);
        if (addr == 0xFFF)
            do_wifi_cmd();
        check_f1_irq();
        return;
    }
    switch (addr)
    {
        case 0x00418:
            printf("[WiFi] Set F1 IRQ mask: $%02X\n", value);
            irq_f1_mask = value;
            check_f1_irq();
            break;
        case 0x00474:
            window_data = (window_data & 0xFFFFFF00) | value;
            break;
        case 0x00475:
            window_data = (window_data & 0xFFFF00FF) | (value << 8);
            break;
        case 0x00476:
            window_data = (window_data & 0xFF00FFFF) | (value << 16);
            break;
        case 0x00477:
            window_data = (window_data & 0x00FFFFFF) | (value << 24);
            break;
        case 0x00478:
            window_write_addr = (window_write_addr & 0xFFFFFF00) | value;
            write_window(window_write_addr, window_data);
            break;
        case 0x00479:
            window_write_addr = (window_write_addr & 0xFFFF00FF) | (value << 8);
            break;
        case 0x0047A:
            window_write_addr = (window_write_addr & 0xFF00FFFF) | (value << 16);
            break;
        case 0x0047B:
            window_write_addr = (window_write_addr & 0x00FFFFFF) | (value << 24);
            break;
        case 0x0047C:
            window_read_addr = (window_read_addr & 0xFFFFFF00) | value;
            window_data = read_window(window_read_addr);
            break;
        case 0x0047D:
            window_read_addr = (window_read_addr & 0xFFFF00FF) | (value << 8);
            break;
        case 0x0047E:
            window_read_addr = (window_read_addr & 0xFF00FFFF) | (value << 16);
            break;
        case 0x0047F:
            window_read_addr = (window_read_addr & 0x00FFFFFF) | (value << 24);
            break;
        default:
            printf("[WiFi] Unrecognized F1 write $%05X: $%02X\n", addr, value);
    }
}

void WiFi::sdio_io_extended()
{
    block.is_write = argument >> 31;
    block.func = (argument >> 28) & 0x7;
    block.block_mode = (argument >> 27) & 0x1;
    block.inc_addr = (argument >> 26) & 0x1;
    block.addr = (argument >> 9) & 0x1FFFF;
    block.count = argument & 0x1FF;
    block.active = true;

    printf("[WiFi] Block transfer - Addr: %d:%05X Count: $%04X\n", block.func, block.addr, block.count);
    printf("Is write: %d Block mode: %d Inc addr: %d\n", block.is_write, block.block_mode, block.inc_addr);

    response[0] = 0x2000;

    if (block.block_mode)
    {
        if (block.count == 0)
            EmuException::die("[Wifi] Infinite block mode active!");

        block.count *= block16_len;
    }
    else if (block.count == 0)
        block.count = 0x200;

    command_end();

    if (block.is_write)
        write_ready();
    else
        read_ready();
}

void WiFi::command_end()
{
    set_istat(0x1);
}

void WiFi::read_ready()
{
    set_istat(1 << 24);
    cdma->set_pending(4);
}

void WiFi::write_ready()
{
    set_istat(1 << 25);
    cdma->set_pending(4);
}

void WiFi::transfer_end()
{
    block.active = false;
    istat &= ~0x1;
    set_istat(1 << 2);
    command_end();
    cdma->clear_pending(4);
}

void WiFi::set_istat(uint32_t value)
{
    uint32_t old_istat = istat;
    istat |= value;

    if (!(old_istat & imask) && (istat & imask))
        send_sdio_interrupt();
}

uint16_t WiFi::read_fifo16()
{
    uint16_t value = 0;
    if (block.active)
    {
        int offset = (block.inc_addr) ? 1 : 0;
        int transfer_amount = 1;
        value = sdio_read_io(block.func, block.addr);

        if (block.count > 1)
        {
            value |= sdio_read_io(block.func, block.addr + offset) << 8;
            transfer_amount++;
        }

        printf("[WiFi] Read FIFO16: $%04X (%d)\n", value, block.count);

        block.addr += offset * transfer_amount;
        block.count -= transfer_amount;
        if (!block.count)
        {
            transfer_end();
            cdma->clear_pending(4);
        }
        else
            cdma->set_pending(4);
    }
    else
        EmuException::die("[WiFi] FIFO read from when block transfer not active!");
    return value;
}

void WiFi::do_wifi_cmd()
{
#ifdef LLE_WIFI
    xtensa_mbox_irq_stat |= 1 << 12;
    if (xtensa_mbox_irq_enable & (1 << 12))
        send_xtensa_soc_irq(12);
#else
    if (!bmi_done)
        do_bmi_cmd();
    else
        do_wmi_cmd();
#endif
}

void WiFi::do_bmi_cmd()
{
    //Bootloader Messaging Interface - commands used to initialize, send, and execute the WiFi card firmware

    static uint8_t lz_tag = 0;
    static bool doing_lz = false;
    static uint32_t lz_addr = 0;

    uint32_t cmd = read32_mbox(mbox[0]);

    switch (cmd)
    {
        case 0x1:
            //DONE
        {
            printf("[WiFi] BMI_DONE\n");
            bmi_done = true;

            uint8_t ready[] = {0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00};
            send_wmi_reply(ready, sizeof(ready), 0, 0, 0);
        }
            break;
        case 0x2:
            //READ_MEMORY
        {
            uint32_t addr = read32_mbox(mbox[0]);
            uint32_t len = read32_mbox(mbox[0]);

            printf("[WiFi] BMI_READ_MEMORY $%08X $%08X\n", addr, len);

            addr &= MEMMAP_MASK;
            addr -= RAM_BASE;

            while (len)
            {
                uint8_t value = RAM[addr];
                write8_mbox(mbox[4], value);
                len--;
            }
        }
            break;
        case 0x3:
            //WRITE_MEMORY
        {
            uint32_t addr = read32_mbox(mbox[0]);
            uint32_t len = read32_mbox(mbox[0]);

            printf("[WiFi] BMI_WRITE_MEMORY $%08X $%08X\n", addr, len);

            addr &= MEMMAP_MASK;
            addr -= RAM_BASE;

            while (len)
            {
                uint8_t value = read8_mbox(mbox[0]);
                RAM[addr] = value;
                addr++;
                len--;
            }
        }
            break;
        case 0x4:
            //EXECUTE
        {
            uint32_t addr = read32_mbox(mbox[0]);
            uint32_t arg = read32_mbox(mbox[0]);

            printf("[WiFi] BMI_EXECUTE $%08X $%08X\n", addr, arg);

            //Return value
            write32_mbox(mbox[4], 0);

            //The boot stub uploaded by NWM reads the EEPROM and copies it into RAM.
            write_window(0x520054, 0x530000); //EEPROM data pointer
            write_window(0x520058, 0x1); //EEPROM ready flag

            memcpy(RAM + 0x10000, eeprom, 0x300);
        }
            break;
        case 0x6:
            //READ_SOC_REGISTER
        {
            uint32_t addr = read32_mbox(mbox[0]);
            printf("[WiFi] BMI_READ_SOC_REGISTER $%08X\n", addr);
            write32_mbox(mbox[4], read_window(addr));
        }
            break;
        case 0x7:
            //WRITE_SOC_REGISTER
        {
            uint32_t addr = read32_mbox(mbox[0]);
            uint32_t value = read32_mbox(mbox[0]);
            printf("[WiFi] BMI_WRITE_SOC_REGISTER $%08X: $%08X\n", addr, value);
            write_window(addr, value);
        }
            break;
        case 0x8:
            //GET_TARGET_INFO
            printf("[WiFi] BMI_GET_TARGET_INFO\n");
            write32_mbox(mbox[4], 0xFFFFFFFF);
            write32_mbox(mbox[4], 0x0000000C);
            write32_mbox(mbox[4], 0x230000B3);
            write32_mbox(mbox[4], 0x00000002);
            break;
        case 0xD:
            //LZ_STREAM_START
        {
            lz_addr = read32_mbox(mbox[0]);
            lz_addr &= MEMMAP_MASK;
            lz_addr -= RAM_BASE;
            printf("[WiFi] BMI_LZ_STREAM_START: $%08X\n", lz_addr);
            doing_lz = false;
        }
            break;
        case 0xE:
        {
            uint32_t len = read32_mbox(mbox[0]);
            printf("[WiFi] BMI_LZ_STREAM_DATA: $%08X\n", len);

            if (!doing_lz)
            {
                doing_lz = true;
                lz_tag = read8_mbox(mbox[0]);
                printf("Tag: $%02X\n", lz_tag);
                len--;
            }

            while (len)
            {
                uint8_t value = read8_mbox(mbox[0]);
                printf("Read LZ stream: $%02X ($%08X)\n", value, lz_addr);
                if (value == lz_tag)
                {
                    uint8_t temp = read8_mbox(mbox[0]);
                    uint32_t bytes = temp;
                    while (temp & 0x80)
                    {
                        bytes &= ~0x80;
                        bytes <<= 7;
                        temp = read8_mbox(mbox[0]);
                        bytes |= temp;
                        len--;
                    }

                    temp = read8_mbox(mbox[0]);
                    uint32_t offset = temp;
                    while (temp & 0x80)
                    {
                        offset &= ~0x80;
                        offset <<= 7;
                        temp = read8_mbox(mbox[0]);
                        offset |= temp;
                        len--;
                    }
                    printf("Decompress $%08X $%08X\n", bytes, offset);
                    len -= 3;
                    if (bytes == 0)
                    {
                        EmuException::die("a");
                        //Literal value
                        RAM[lz_addr] = value;
                        lz_addr++;
                    }
                    else
                    {
                        for (unsigned int i = 0; i < bytes; i++)
                        {
                            RAM[lz_addr] = RAM[lz_addr - offset];
                            lz_addr++;
                        }
                    }
                }
                else
                {
                    RAM[lz_addr] = value;
                    lz_addr++;
                    len--;
                }
            }
        }
            break;
        default:
            EmuException::die("[WiFi] Unrecognized BMI command $%02X", cmd);
    }
}

void WiFi::do_wmi_cmd()
{
    uint16_t header = read16_mbox(mbox[0]);
    uint16_t len = read16_mbox(mbox[0]);
    uint16_t header2 = read16_mbox(mbox[0]);

    uint16_t cmd = read16_mbox(mbox[0]);

    switch (cmd)
    {
        case 0x0002:
            //RECONNECT
        {
            uint16_t service = read16_mbox(mbox[0]);
            uint16_t flags = read16_mbox(mbox[0]);

            uint8_t reply[10];
            memset(reply, 0, sizeof(reply));

            *(uint16_t*)&reply[0] = 0x0003;
            *(uint16_t*)&reply[2] = service;

            reply[5] = (service & 0xFF) + 1;
            *(uint32_t*)&reply[6] = 0x00010001;

            send_wmi_reply(reply, sizeof(reply), 0, 0, 0);
            printf("[WiFi] WMI_RECONNECT: $%04X $%04X\n", service, flags);
        }
            break;
        case 0x0004:
            //SYNCHRONIZE
        {
            uint8_t reply[18];
            memset(reply, 0, sizeof(reply));

            *(uint16_t*)&reply[0] = 0x1001; //WMI_READY event
            //*(uint16_t*)&reply[0] = 0;

            memcpy(reply + 2, mac, 6);

            *(uint16_t*)&reply[8] = 0x0602;
            *(uint32_t*)&reply[10] = 0x230000EC;

            //*(uint32_t*)&reply[14] = 0xDEADBEEF;

            send_wmi_reply(reply, sizeof(reply), 1, 0, 0);

            auto blorp = [this](uint64_t param)
            {
                uint8_t reply[8];
                memset(reply, 0, sizeof(reply));

                *(uint16_t*)&reply[0] = 0x000E; //WMI_GET_CHANNEL_LIST
                *(uint32_t*)&reply[4] = 1;
                *(uint16_t*)&reply[6] = 0;

                send_wmi_reply(reply, sizeof(reply), 1, 0, 0);

                check_f1_irq();
                printf("hey\n");
            };

            scheduler->add_event(blorp, XTENSA_CLOCKRATE, 500000);

            printf("[WiFi] WMI_SYNCHRONIZE\n");
        }
            break;
        default:
            EmuException::die("[WiFi] Unrecognized WMI command $%02X", cmd);
    }

    //Remove all remaining data from the mbox
    std::queue<uint8_t> empty;
    mbox[0].swap(empty);
}

void WiFi::send_wmi_reply(uint8_t *reply, uint32_t len, uint8_t eid, uint8_t flag, uint16_t ctrl)
{
    uint32_t total_len = len + 6;

    write8_mbox(mbox[4], eid);
    write8_mbox(mbox[4], flag);

    write16_mbox(mbox[4], len & 0xFFFF);
    write16_mbox(mbox[4], ctrl);

    for (unsigned int i = 0; i < len; i++)
        write8_mbox(mbox[4], reply[i]);

    if (flag & 0x2)
    {
        //Trailer
        total_len += ctrl;
        for (unsigned int i = 0; i < ctrl; i++)
            write8_mbox(mbox[4], 0);
    }

    //Align to a 128-byte boundary by padding with zeroes
    while (total_len & 0x7F)
    {
        write8_mbox(mbox[4], 0);
        total_len++;
    }
}

void WiFi::send_xtensa_soc_irq(int id)
{
    xtensa_irq_stat |= 1 << id;
    xtensa.send_irq(16 - id);
}

void WiFi::check_card_irq()
{
    bool new_card_irq = card_irq_stat && !card_irq_mask;
    if (!old_card_irq && new_card_irq)
    {
        printf("[WiFi] Card IRQ!\n");
        send_sdio_interrupt();
    }

    old_card_irq = new_card_irq;
}

void WiFi::check_f0_irq()
{
    irq_f0_stat = 0;

    if (irq_f1_stat & irq_f1_mask)
        irq_f0_stat |= 1 << 1;

    if (irq_f0_mask & 0x1)
        card_irq_stat = (irq_f0_mask & irq_f0_stat) != 0;

    check_card_irq();
}

void WiFi::check_f1_irq()
{
    irq_f1_stat = 0;

    for (int i = 0; i < 4; i++)
        irq_f1_stat |= (mbox[i + 4].size() != 0) << i;

    check_f0_irq();
}

void WiFi::write_fifo16(uint16_t value)
{
    if (block.active)
    {
        printf("[WiFi] Write FIFO16: $%04X\n", value);
        int offset = (block.inc_addr) ? 1 : 0;
        int transfer_amount = 1;
        sdio_write_io(block.func, block.addr, value & 0xFF);

        if (block.count > 1)
        {
            sdio_write_io(block.func, block.addr + offset, value >> 8);
            transfer_amount++;
        }

        block.addr += offset * transfer_amount;
        block.count -= transfer_amount;
        printf("[WiFi] Count left: %d\n", block.count);
        if (!block.count)
        {
            transfer_end();
        }
        else
            cdma->set_pending(4);
    }
    else
    {
        EmuException::die("[WiFi] FIFO written to when block transfer not active!");
    }
}

uint32_t WiFi::read_window(uint32_t addr)
{
    printf("[WiFi] Read window $%08X\n", addr);
    return read32_xtensa(addr);
}

void WiFi::write_window(uint32_t addr, uint32_t value)
{
    printf("[WiFi] Write window $%08X: $%08X\n", addr, value);
    if (addr == 2)
        return;
    write32_xtensa(addr, value);
}

void WiFi::set_sdio_interrupt_handler(std::function<void ()> func)
{
    send_sdio_interrupt = func;
}

uint16_t WiFi::read16(uint32_t addr)
{
    addr &= 0xFFF;
    uint16_t reg = 0;
    printf("[WiFi] Read16 $%08X\n", addr);
    if (addr >= 0x00C && addr < 0x01C)
    {
        int index = ((addr - 0x00C) / 4) & 0x3;
        if (addr % 4 == 2)
            reg = response[index] >> 16;
        else
            reg = response[index] & 0xFFFF;
        return reg;
    }
    switch (addr)
    {
        case 0x01C:
            reg = istat & 0xFFFF;
            reg |= 1 << 5; //always inserted
            printf("[WiFi] Read ISTAT_L: $%04X\n", reg);
            break;
        case 0x01E:
            reg = istat >> 16;
            printf("[WiFi] Read ISTAT_H: $%04X\n", reg);
            break;
        case 0x020:
            reg = imask & 0xFFFF;
            break;
        case 0x022:
            reg = imask >> 16;
            break;
        case 0x026:
            reg = block16_len;
            break;
        case 0x02C:
        case 0x02E:
            //Error register
            return 0;
        case 0x030:
            return read_fifo16();
        case 0x036:
            return card_irq_stat;
        case 0x038:
            return card_irq_mask;
        case 0x100:
            reg |= data32_irq.data32_mode << 1;
            reg |= data32_irq.rx32rdy_irq_enable << 11;
            reg |= data32_irq.tx32rq_irq_enable << 12;
            break;
        default:
            printf("[WiFi] Unrecognized read16 $%08X\n", addr);
    }
    return reg;
}

void WiFi::write16(uint32_t addr, uint16_t value)
{
    addr &= 0xFFF;
    switch (addr)
    {
        case 0x000:
            do_sdio_cmd(value & 0x3F);
            break;
        case 0x004:
            argument &= ~0xFFFF;
            argument |= value;
            break;
        case 0x006:
            argument &= 0xFFFF;
            argument |= value << 16;
            break;
        case 0x01C:
            istat &= value | (istat & 0xFFFF0000);
            break;
        case 0x01E:
            istat &= (value << 16) | (istat & 0xFFFF);
            break;
        case 0x020:
            imask &= ~0xFFFF;
            imask |= value;
            printf("[WiFi] Write IMASK_L: $%04X\n", value);
            break;
        case 0x022:
            imask &= 0xFFFF;
            imask |= value << 16;
            printf("[WiFi] Write IMASK_H: $%04X\n", value);
            break;
        case 0x026:
            block16_len = value;
            if (block16_len > 0x200)
                block16_len = 0x200;
            break;
        case 0x030:
            write_fifo16(value);
            break;
        case 0x036:
            card_irq_stat &= ~(value & 0x1);
            old_card_irq = false;
            break;
        case 0x038:
            printf("[WiFi] Card IRQ mask: $%04X\n", value);
            card_irq_mask = value & 0x1;
            check_card_irq();
            break;
        case 0x100:
            data32_irq.data32_mode = (value >> 1) & 0x1;
            data32_irq.rx32rdy_irq_enable = (value >> 11) & 0x1;
            data32_irq.tx32rq_irq_enable = (value >> 12) & 0x1;
            printf("[WiFi] Set SD_DATA32_IRQ: $%04X\n", value);
            break;
        default:
            printf("[WiFi] Unrecognized write16 $%08X: $%04X\n", addr, value);
    }
}

uint8_t WiFi::read8_xtensa(uint32_t addr)
{
    addr &= MEMMAP_MASK;
    if (addr >= ROM_BASE && addr < RAM_BASE)
        return ROM[addr - ROM_BASE];
    if (addr >= RAM_BASE && addr < RAM_BASE + 0x20000)
        return RAM[addr - RAM_BASE];

    EmuException::die("[WiFi] Unrecognized Xtensa read8 $%08X\n", addr);
    return 0;
}

uint16_t WiFi::read16_xtensa(uint32_t addr)
{
    addr &= MEMMAP_MASK;
    if (addr >= ROM_BASE && addr < RAM_BASE)
        return *(uint16_t*)&ROM[addr - ROM_BASE];
    if (addr >= RAM_BASE && addr < RAM_BASE + 0x20000)
        return *(uint16_t*)&RAM[addr - RAM_BASE];

    EmuException::die("[WiFi] Unrecognized Xtensa read16 $%08X\n", addr);
    return 0;
}

uint32_t WiFi::read32_xtensa(uint32_t addr)
{
    addr &= MEMMAP_MASK;
    if (addr >= ROM_BASE && addr < RAM_BASE)
        return *(uint32_t*)&ROM[addr - ROM_BASE];
    if (addr >= RAM_BASE && addr < RAM_BASE + 0x20000)
        return *(uint32_t*)&RAM[addr - RAM_BASE];

    //Patch enable
    if (addr >= 0x8000 && addr < 0x8080)
        return 0;

    if (addr >= 0x18080 && addr < 0x180A0)
    {
        printf("[WiFi] Read32 Xtensa WLAN_LOCAL_COUNT $%08X\n", addr);
        return 0;
    }

    switch (addr)
    {
        case 0x04000:
            //Reset control
            return 0;
        case 0x04028:
            //Clock control?
            return 0;
        case 0x04030:
            //Watchdog control
            return 0;
        case 0x04044:
            return xtensa_irq_stat;
        case 0x04048:
        case 0x04058:
        case 0x04068:
        case 0x04078:
            return timers.read_target((addr - 0x4048) / 0x10);
        case 0x0404C:
        case 0x0405C:
        case 0x0406C:
        case 0x0407C:
            return timers.read_count((addr - 0x0404C) / 0x10);
        case 0x04050:
        case 0x04060:
        case 0x04070:
        case 0x04080:
            return timers.read_ctrl((addr - 0x4050) / 0x10);
        case 0x04054:
        case 0x04064:
        case 0x04074:
        case 0x04084:
            return timers.read_int_status((addr - 0x4054) / 0x10);
        case 0x04094:
            return timers.read_ctrl(4);
        case 0x04098:
            return timers.read_int_status(4);
        case 0x040C0:
            //SOC_RESET_CAUSE
            return 0x2;
        case 0x040C4:
            //SOC_SYSTEM_SLEEP
            return 0;
        case 0x040EC:
            //Chip id
            return 0x0D000001;
        case 0x040F0:
            return 0;
        case 0x04110:
            //Power control
            return 0;
        case 0x14048:
            printf("[WiFi] Read32 Xtensa GPIO_PIN8\n");
            return 0;
        case 0x180C0:
            //Local scratchpad
            return 0;
    }

    EmuException::die("[WiFi] Unrecognized Xtensa read32 $%08X\n", addr);
    return 0;
}

void WiFi::write8_xtensa(uint32_t addr, uint8_t value)
{
    addr &= MEMMAP_MASK;
    if (addr >= RAM_BASE && addr < RAM_BASE + 0x20000)
    {
        RAM[addr - RAM_BASE] = value;
        return;
    }

    EmuException::die("[WiFi] Unrecognized Xtensa write8 $%08X: $%02X\n", addr, value);
}

void WiFi::write16_xtensa(uint32_t addr, uint16_t value)
{
    addr &= MEMMAP_MASK;
    if (addr >= RAM_BASE && addr < RAM_BASE + 0x20000)
    {
        *(uint16_t*)&RAM[addr - RAM_BASE] = value;
        return;
    }

    EmuException::die("[WiFi] Unrecognized Xtensa write16 $%08X: $%04X\n", addr, value);
}

void WiFi::write32_xtensa(uint32_t addr, uint32_t value)
{
    addr &= MEMMAP_MASK;
    if (addr >= RAM_BASE && addr < RAM_BASE + 0x20000)
    {
        *(uint32_t*)&RAM[addr - RAM_BASE] = value;
        return;
    }

    if (addr >= 0x8000 && addr < 0x8080)
    {
        printf("[WiFi] Write32 Xtensa MC_TCAM_VALID $%08X: $%08X\n", addr, value);
        return;
    }

    if (addr >= 0x18080 && addr < 0x180A0)
    {
        printf("[WiFi] Write32 Xtensa WLAN_LOCAL_COUNT $%08X: $%08X\n", addr, value);
        return;
    }

    if (addr >= 0x180A0 && addr < 0x180C0)
    {
        printf("[WiFi] Write32 Xtensa WLAN_COUNT_INC $%08X: $%08X\n", addr, value);
        return;
    }

    switch (addr)
    {
        case 0x04000:
            printf("[WiFi] Write32 Xtensa SOC_RESET_CONTROL: $%08X\n", value);
            reset();
            return;
        case 0x04014:
            //Clock control?
            return;
        case 0x04020:
            //SOC_CPU_CLOCK
            return;
        case 0x04028:
            //SOC_CPU_CLOCK_CONTROL
            return;
        case 0x04030:
            //Watchdog control
            return;
        case 0x04048:
        case 0x04058:
        case 0x04068:
        case 0x04078:
            timers.write_target((addr - 0x4048) / 0x10, value);
            return;
        case 0x04050:
        case 0x04060:
        case 0x04070:
        case 0x04080:
            timers.write_ctrl((addr - 0x4050) / 0x10, value);
            return;
        case 0x04054:
        case 0x04064:
        case 0x04074:
        case 0x04084:
            timers.write_int_status((addr - 0x4054) / 0x10, value);
            return;
        case 0x04088:
            timers.write_target(4, value);
            return;
        case 0x04094:
            timers.write_ctrl(4, value);
            return;
        case 0x04098:
            timers.write_int_status(4, value);
            return;
        case 0x040C4:
            printf("[WiFi] Write32 Xtensa SOC_SYSTEM_SLEEP: $%08X\n", value);
            return;
        case 0x040D4:
            printf("[WiFi] Write32 Xtensa SOC_LPO_CAL_TIME: $%08X\n", value);
            return;
        case 0x040D8:
            printf("[WiFi] Write32 Xtensa SOC_LPO_INIT_DIVIDEND_INT: $%08X\n", value);
            return;
        case 0x040DC:
            printf("[WiFi] Write32 Xtensa SOC_LPO_INIT_DIVIDENT_FRACTION: $%08X\n", value);
            return;
        case 0x040F0:
            //More clock control
            return;
        case 0x04110:
            //Power control
            return;
        case 0x08200:
            //Memory error control
            return;
        case 0x14010:
            printf("[WiFi] Write32 Xtensa WLAN_GPIO_ENABLE_W1TS: $%08X\n", value);
            return;
        case 0x14048:
            printf("[WiFi] Write32 Xtensa GPIO_PIN8: $%08X\n", value);
            return;
        case 0x18058:
            printf("[WiFi] Write32 Xtensa WLAN_MBOX_INT_STATUS: $%08X\n", value);
            xtensa_mbox_irq_stat &= ~value;
            return;
        case 0x1805C:
            printf("[WiFi] Write32 Xtensa WLAN_MBOX_INT_ENABLE: $%08X\n", value);
            xtensa_mbox_irq_enable = value;
            return;
        case 0x180C0:
            printf("[WiFi] Write32 Xtensa LOCAL_SCRATCH[0]: $%08X\n", value);
            return;
        case 0x180E4:
            //Some sort of SDIO config?
            return;
    }

    EmuException::die("[WiFi] Unrecognized Xtensa write32 $%08X: $%08X\n", addr, value);
}

uint32_t WiFi::read_fifo32()
{
    uint32_t value = 0;
    value = read_fifo16();
    if (block.active)
        value |= read_fifo16() << 16;
    return value;
}

void WiFi::write_fifo32(uint32_t value)
{
    write_fifo16(value & 0xFFFF);
    if (block.active)
        write_fifo16(value >> 16);
}
