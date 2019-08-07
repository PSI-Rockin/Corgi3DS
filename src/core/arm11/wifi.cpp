#include "../common/common.hpp"
#include "../corelink_dma.hpp"
#include "wifi.hpp"

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

WiFi::WiFi(Corelink_DMA* cdma) : cdma(cdma)
{
    send_sdio_interrupt = nullptr;
}

void WiFi::reset()
{
    block.active = false;
    eeprom_ready = false;
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
    if (!bmi_done)
        do_bmi_cmd();
    else
        do_wmi_cmd();
}

void WiFi::do_bmi_cmd()
{
    //Bootloader Messaging Interface - commands used to initialize, send, and execute the WiFi card firmware

    uint32_t cmd = read32_mbox(mbox[0]);

    switch (cmd)
    {
        case 0x1:
            //DONE
        {
            printf("[WiFi] BMI_DONE\n");
            eeprom_ready = true;
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

            while (len)
            {
                uint8_t value = 0;
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

            while (len)
            {
                uint8_t value = read8_mbox(mbox[0]);
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
        }
            break;
        case 0x6:
            //READ_SOC_REGISTER
        {
            uint32_t addr = read32_mbox(mbox[0]);
            write32_mbox(mbox[4], read_window(addr));
        }
            break;
        case 0x7:
            //WRITE_SOC_REGISTER
        {
            uint32_t addr = read32_mbox(mbox[0]);
            uint32_t value = read32_mbox(mbox[0]);
            write_window(addr, value);
        }
            break;
        case 0x8:
            //GET_TARGET_INFO
            write32_mbox(mbox[4], 0xFFFFFFFF);
            write32_mbox(mbox[4], 0x0000000C);
            write32_mbox(mbox[4], 0x230000B3);
            write32_mbox(mbox[4], 0x00000002);
            break;
        case 0xD:
            //LZ_STREAM_START
        {
            uint32_t start = read32_mbox(mbox[0]);
            printf("[WiFi] BMI_LZ_STREAM_START: $%08X\n", start);
        }
            break;
        case 0xE:
        {
            uint32_t len = read32_mbox(mbox[0]);
            printf("[WiFi] BMI_LZ_STREAM_DATA: $%08X\n", len);

            while (len)
            {
                uint8_t value = read8_mbox(mbox[0]);
                len--;
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

            memcpy(reply + 2, mac, 6);

            reply[8] = 0x2;
            *(uint32_t*)&reply[10] = 0x230000B3;

            //memset(reply + 2, 0xFF, 12);

            *(uint32_t*)&reply[14] = 0x00010001;

            send_wmi_reply(reply, sizeof(reply), 1, 0, 0);
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

    //Align to a 128-byte boundary by padding with zeroes
    while (total_len & 0x7F)
    {
        write8_mbox(mbox[4], 0);
        total_len++;
    }
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
    printf("[WiFi] Read window: $%08X\n", addr);
    uint32_t value = 0;
    if (addr >= 0x1FFC00 && addr < 0x200000)
        return *(uint32_t*)&eeprom[addr & 0x3FF];
    switch (addr)
    {
        case 0x0040C0:
            //Reset cause - this indicates a cold boot
            return 0x2;
        case 0x0040EC:
            //Chip ID associated with AR6014
            return 0x0D000001;
        case 0x520054:
            //EEPROM base pointer
            return 0x001FFC00;
        case 0x520058:
            return eeprom_ready;
        default:
            printf("[WiFi] Unrecognized window read $%08X\n", addr);
    }
    return value;
}

void WiFi::write_window(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        default:
            printf("[WiFi] Unrecognized window write $%08X: $%08X\n", addr, value);
    }
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
    printf("Write FIFO32: $%08X\n", value);
    write_fifo16(value & 0xFFFF);
    if (block.active)
        write_fifo16(value >> 16);
}
