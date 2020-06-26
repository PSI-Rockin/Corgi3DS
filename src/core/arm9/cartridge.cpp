#include <cstdio>
#include <cstring>
#include "../common/common.hpp"
#include "cartridge.hpp"
#include "dma9.hpp"
#include "interrupt9.hpp"

Cartridge::Cartridge(DMA9* dma9, Interrupt9* int9) : dma9(dma9), int9(int9)
{
    save_data = nullptr;
}

Cartridge::~Cartridge()
{
    card.close();
    delete[] save_data;
}

void Cartridge::reset()
{
    if (!card.is_open())
    {
        cart_id = 0xFFFFFFFF;
        save_id = 0xFFFFFFFF;
    }

    read_block_count = 0;
    ntr_enable = 0;
    data_pos = 0;
    data_bytes_left = 0;

    ntr_romctrl.data_ready = false;
    ntr_romctrl.busy = false;

    ctr_romctrl.data_ready = false;
    ctr_romctrl.busy = false;
    card2_active = false;

    spi_input_pos = 0;
    spi_output_pos = 0;
    spi_block_len = 0;

    save_dirty = false;

    spi_state = SPICARD_STATE::IDLE;
}

void Cartridge::save_check()
{
    if (card_inserted() && save_dirty)
    {
        printf("Save check!");
        std::ofstream save_file(save_file_name, std::ios::binary);
        save_file.write((char*)save_data, save_size);
        save_file.close();
    }
}

bool Cartridge::mount(std::string file_name)
{
    if (card.is_open())
        card.close();
    card.open(file_name, std::ios::binary | std::ios::in | std::ios::out);

    cart_id = 0xFFFFFFFF;
    save_id = 0xFFFFFFFF;

    if (card.is_open())
    {
        if (save_data)
            delete[] save_data;

        //Detect if this is a Card2 save card. If so, no need to load a savefile.
        card.read((char*)data_buffer, 0x400);
        is_card2 = *(uint32_t*)&data_buffer[0x200] != 0xFFFFFFFF;

        if (!is_card2)
            init_card1_save(file_name);

        //Note: this assumes the cartridge is a 3DS cart
        cart_id = 0x900000C2;

        //We must calculate the size of the cart. HOS will refuse to read from cart sectors larger than the
        //indicated size, and I don't want to risk any copy protection problems from using absurd sizes.
        constexpr static uint8_t SIZE_BYTES[] = {0x7F, 0xFF, 0xFE, 0xFA, 0xF8, 0xF0, 0xE0, 0xE1, 0xE2};
        uint8_t size_index = 0;

        card.seekg(0, std::ios::end);
        size_t size = card.tellg();
        uint64_t compare_size = 1024 * 1024 * 128;
        while (size > compare_size)
        {
            compare_size <<= 1;
            size_index++;

            //File goes beyond the supported sizes, error out. Bad user.
            if (size_index >= sizeof(SIZE_BYTES))
                return false;
        }

        cart_id |= SIZE_BYTES[size_index] << 8;
        cart_id |= is_card2 << 27;
        printf("[Cartridge] Calculated cart ID: $%08X\n", cart_id);
        return true;
    }
    return false;
}

void Cartridge::init_card1_save(std::string file_name)
{
    //Find the start of the extension of the card file and generate our save file name from the rest.
    unsigned int extension_start = file_name.length() - 1;
    while (file_name[extension_start] != '.')
        extension_start--;

    save_file_name = file_name.substr(0, extension_start) + ".sav";
    printf("Save name: %s\n", save_file_name.c_str());

    //Attempt to load a save, if possible
    save_data = new uint8_t[1024 * 1024 * 8];
    memset(save_data, 0xFF, 1024 * 1024 * 8);

    //Byte 0 = capacity (0x11 = 128 KB, 0x13 = 512 KB, 0x17 = 8 MB. Process9 rejects all others)
    //Byte 1 = device type (0x22 = flash?)
    //Byte 2 = manufacturer (0xC2 = Macronix)
    save_id = 0x0022C2;
    std::ifstream save_file(save_file_name, std::ios::ate | std::ios::binary);
    if (save_file.is_open())
    {
        //Save found. First we check the size.
        save_size = save_file.tellg();

        if (save_size == 1024 * 128)
            save_id |= 0x11 << 16;
        else if (save_size == 1024 * 512)
            save_id |= 0x13 << 16;
        else if (save_size == 1024 * 1024 * 8)
            save_id |= 0x17 << 16;
        else
            printf("[SPICARD] WARNING: Save size is not 128 KB, 512 KB, or 8 MB and thus will not be loaded");

        if (save_id & 0x00FF0000)
        {
            save_file.seekg(0);
            save_file.read((char*)save_data, save_size);
        }

        save_file.close();
    }
    else
    {
        //TODO: The cartridge exheader contains the save size, but it is encrypted.
        //Until we figure out a way to decrypt it, we'll just use a hardcoded size.
        save_size = 1024 * 128;
        save_id = 0x1122C2;
    }
}

bool Cartridge::card_inserted()
{
    return card.is_open();
}

void Cartridge::process_ntr_cmd()
{
    data_pos = 0;
    switch (cmd_buffer[0])
    {
        case 0x3E:
            ntr_romctrl.busy = false;
            ntr_romctrl.data_ready = false;
            break;
        case 0x71:
            ntr_romctrl.busy = false;
            ntr_romctrl.data_ready = false;
            break;
        case 0x90:
            ntr_romctrl.data_ready = true;
            *(uint32_t*)&data_buffer[0] = cart_id;
            data_bytes_left = 0x4;
            break;
        case 0x9F:
            ntr_romctrl.data_ready = true;

            //Output is all high-Z
            memset(data_buffer, 0xFF, 0x2000);

            data_bytes_left = 0x2000;
            break;
        case 0xA0:
            ntr_romctrl.data_ready = true;
            memset(data_buffer, 0, 0x2000);
            data_bytes_left = 0x4;
            break;
        default:
            EmuException::die("[NTRCARD] Unrecognized command $%02X\n", cmd_buffer[0]);
    }
}

void Cartridge::process_ctr_cmd()
{
    data_pos = 0;
    switch (cmd_buffer[0])
    {
        case 0x82:
            //Read header
            ctr_romctrl.data_ready = true;

            card.seekg(0x1000);
            card.read((char*)data_buffer, 0x200);
            data_bytes_left = 0x200;
            break;
        case 0x83:
            //Seed
            ctr_romctrl.data_ready = false;
            ctr_romctrl.busy = false;
            break;
        case 0xA2:
            //Cart ID
            ctr_romctrl.data_ready = true;
            *(uint32_t*)&data_buffer[0] = cart_id;
            data_bytes_left = 0x4;
            break;
        case 0xA3:
            //Unknown
            ctr_romctrl.data_ready = true;
            memset(data_buffer, 0, 0x2000);
            data_bytes_left = 0x4;
            break;
        case 0xBF:
            //Read
            read_addr = bswp32(*(uint32_t*)&cmd_buffer[4]);
            printf("[CTRCARD] Reading from $%08X\n", read_addr);
            ctr_romctrl.data_ready = true;
            card.seekg(read_addr);
            data_bytes_left = read_block_count * 0x200;

            if (data_bytes_left >= 0x1000)
                card.read((char*)data_buffer, 0x1000);
            else
                card.read((char*)data_buffer, data_bytes_left);
            break;
        case 0xC3:
            //Card2: Set write address
            card2_write_addr = bswp32(*(uint32_t*)&cmd_buffer[4]);
            data_pos = 0;
            ctr_romctrl.data_ready = true;
            data_bytes_left = bswp32(*(uint32_t*)&cmd_buffer[12]) * 0x200;
            ctr_romctrl.busy = false;
            card2_active = true;
            card.seekg(card2_write_addr);
            printf("[CTRCARD] Card2 start write (addr: $%llX, bytes: $%08X)\n", card2_write_addr, data_bytes_left);
            break;
        case 0xC4:
            //Card2: Unknown
            ctr_romctrl.data_ready = true;
            ctr_romctrl.busy = false;
            break;
        case 0xC5:
            //Unknown
            ctr_romctrl.data_ready = false;
            ctr_romctrl.busy = false;
            break;
        case 0xC6:
            //Read unique ID - dunno what to put here
            memset(data_buffer, 0, 0x40);
            data_bytes_left = 0x40;
            ctr_romctrl.data_ready = true;
            break;
        case 0xC7:
            //Card2: Get write status? Sent after 0x200 bytes have been transferred for 0xC3
            printf("[CTRCARD] Card2 flush\n");
            data_pos = 0;
            data_bytes_left += 4;
            data_buffer[0] = card2_active;
            ctr_romctrl.busy = false;
            ctr_romctrl.data_ready = true;
            card.flush();
            break;
        default:
            EmuException::die("[CTRCARD] Unrecognized command $%02X\n", cmd_buffer[0]);
    }
}

void Cartridge::process_spicard_cmd()
{
    switch (spi_state)
    {
        case SPICARD_STATE::IDLE:
            printf("[SPICARD] Selected!\n");
            spi_state = SPICARD_STATE::SELECTED;
            break;
        case SPICARD_STATE::SELECTED:
            spi_cmd = spi_input_buffer[0];

            switch (spi_cmd)
            {
                case 0x02:
                    spi_state = SPICARD_STATE::WRITE_READY;

                    spi_save_addr = spi_input_buffer[1] << 16;
                    spi_save_addr |= spi_input_buffer[2] << 8;
                    spi_save_addr |= spi_input_buffer[3];

                    printf("[SPICARD] Writing $%08X\n", spi_save_addr);
                    break;
                case 0x03:
                    spi_state = SPICARD_STATE::SELECTED;
                    spi_save_addr = spi_input_buffer[1] << 16;
                    spi_save_addr |= spi_input_buffer[2] << 8;
                    spi_save_addr |= spi_input_buffer[3];
                    memcpy(spi_output_buffer, save_data + spi_save_addr,
                           std::min(spi_block_len, (int)sizeof(spi_output_buffer)));
                    printf("[SPICARD] Reading from $%08X\n", spi_save_addr);
                    break;
                case 0x05:
                    //Read status register
                    //Bit 0=write/prog/erase in progress
                    //Bit 1=write enabled
                    *(uint32_t*)&spi_output_buffer[0] = 1 << 1;
                    break;
                case 0x06:
                    //Enable writes
                    break;
                case 0x9F:
                    //Read save chip ID
                    spi_state = SPICARD_STATE::SELECTED;
                    *(uint32_t*)&spi_output_buffer[0] = save_id;
                    break;
                case 0xEB:
                    spi_state = SPICARD_STATE::NEEDS_PARAMS;
                    break;
                default:
                    EmuException::die("[SPICARD] Unrecognized SELECTED cmd $%02X", spi_cmd);
            }
            break;
        case SPICARD_STATE::NEEDS_PARAMS:
            switch (spi_cmd)
            {
                case 0xEB:
                    spi_state = SPICARD_STATE::SELECTED;
                    spi_save_addr = spi_input_buffer[0] << 16;
                    spi_save_addr |= spi_input_buffer[1] << 8;
                    spi_save_addr |= spi_input_buffer[2];
                    printf("[SPICARD] Reading from $%08X\n", spi_save_addr);
                    memcpy(spi_output_buffer, save_data + spi_save_addr,
                           std::min(spi_block_len, (int)sizeof(spi_output_buffer)));
                    break;
                default:
                    EmuException::die("[SPICARD] Unrecognized NEEDS_PARAMS cmd $%02X", spi_cmd);
            }
            break;
        case SPICARD_STATE::WRITE_READY:
        case SPICARD_STATE::PROGRAM_READY:
            EmuException::die("[SPICARD] WRITE/PROGRAM/ERASE_READY should never be triggered!");
            break;
    }
}

uint16_t Cartridge::read16_ntr(uint32_t addr)
{
    uint16_t reg = 0;
    switch (addr)
    {
        case 0x10164000:
            return ntr_enable;
        default:
            printf("[NTRCARD] Unrecognized read16 $%08X\n", addr);
    }
    return reg;
}

uint32_t Cartridge::read32_ntr(uint32_t addr)
{
    uint32_t reg = 0;
    switch (addr)
    {
        case 0x10164004:
            reg |= ntr_romctrl.data_ready << 23;
            reg |= ntr_romctrl.busy << 31;
            break;
        case 0x1016401C:
            reg = *(uint32_t*)&data_buffer[data_pos];
            data_bytes_left -= 4;
            data_pos += 4;
            if (!data_bytes_left)
            {
                //TODO: Signal some interrupt?
                ntr_romctrl.busy = false;
                ntr_romctrl.data_ready = false;
            }
            break;
        default:
            printf("[NTRCARD] Unrecognized read32 $%08X\n", addr);
    }
    return reg;
}

uint32_t Cartridge::read32_ctr(uint32_t addr)
{
    uint32_t reg = 0;
    switch (addr)
    {
        case 0x10004000:
            reg |= ctr_romctrl.data_ready << 27;
            reg |= ctr_romctrl.write_mode << 29;
            reg |= ctr_romctrl.irq_enable << 30;
            reg |= ctr_romctrl.busy << 31;
            //printf("[CTRCARD] Read32 ROMCTRL: $%08X\n", reg);
            break;
        case 0x10004008:
            reg = ctr_secctrl | (1 << 14);
            printf("[CTRCARD] Read32 SECCTRL: $%08X\n", reg);
            break;
        case 0x10004030:
            reg = *(uint32_t*)&data_buffer[data_pos];
            data_bytes_left -= 4;
            data_pos += 4;
            if (data_pos == 0x20)
                dma9->set_ndma_req(NDMA_CTRCARD0);
            if (!data_bytes_left)
            {
                ctr_romctrl.busy = false;
                ctr_romctrl.data_ready = false;
                dma9->clear_ndma_req(NDMA_CTRCARD0);

                if (ctr_romctrl.irq_enable)
                    int9->assert_irq(23);
            }
            else if (data_pos == 0x1000)
            {
                data_pos = 0;
                card.read((char*)data_buffer, 0x1000);
                dma9->clear_ndma_req(NDMA_CTRCARD0);
            }
            //printf("[CTRCARD] Read32 output FIFO: $%08X\n", reg);
            break;
        default:
            printf("[CTRCARD] Unrecognized read32 $%08X\n", addr);
    }
    return reg;
}

uint32_t Cartridge::read32_spicard(uint32_t addr)
{
    if (!card_inserted())
        return 0;
    uint32_t reg = 0;
    switch (addr)
    {
        case 0x1000D80C:
            if (spi_block_len <= 0)
                EmuException::die("[SPICARD] Read from FIFO when no data is present!");
            reg = *(uint32_t*)&spi_output_buffer[spi_output_pos];
            spi_output_pos += 0x4;
            spi_block_len -= 4;
            if (spi_output_pos >= sizeof(spi_output_buffer) && spi_block_len > 0)
            {
                spi_output_pos = 0;
                switch (spi_cmd)
                {
                    case 0x03:
                    case 0xEB:
                        //Read
                        spi_save_addr += sizeof(spi_output_buffer);
                        memcpy(spi_output_buffer, save_data + spi_save_addr,
                               std::min(spi_block_len, (int)sizeof(spi_output_buffer)));
                        break;
                    default:
                        EmuException::die("[SPICARD] Unrecognized command in FIFO read $%02X", spi_cmd);
                }
            }
            //printf("[SPICARD] Read32 NSPI_FIFO: $%08X\n", reg);
            break;
        default:
            printf("[SPICARD] Unrecognized read32 $%08X\n", addr);
    }
    return reg;
}

void Cartridge::write8_ntr(uint32_t addr, uint8_t value)
{
    if (addr >= 0x10164008 && addr < 0x10164010)
    {
        printf("[NTRCARD] Write cmd $%08X: $%02X\n", addr, value);
        cmd_buffer[addr & 0x7] = value;
        return;
    }
    switch (addr)
    {
        default:
            printf("[NTRCARD] Unrecognized write8 $%08X: $%02X\n", addr, value);
    }
}

void Cartridge::write16_ntr(uint32_t addr, uint16_t value)
{
    switch (addr)
    {
        case 0x10164000:
            printf("[NTRCARD] Write16 CARDMCNT: $%04X\n", value);
            ntr_enable = value;
            break;
        default:
            printf("[NTRCARD] Unrecognized write16 $%08X: $%08X\n", addr, value);
    }
}

void Cartridge::write32_ntr(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        case 0x10164004:
            printf("[NTRCARD] Write32 ROMCTRL: $%08X\n", value);
            if (!ntr_romctrl.busy && (value & (1 << 31)))
            {
                ntr_romctrl.busy = true;
                process_ntr_cmd();
            }
            break;
        case 0x10164008:
            printf("[NTRCARD] Write32 cmd $%08X: $%08X\n", addr, value);
            *(uint32_t*)&cmd_buffer[0] = value;
            break;
        case 0x1016400C:
            printf("[NTRCARD] Write32 cmd $%08X: $%08X\n", addr, value);
            *(uint32_t*)&cmd_buffer[4] = value;
            break;
        default:
            printf("[NTRCARD] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}

void Cartridge::write8_ctr(uint32_t addr, uint8_t value)
{
    switch (addr)
    {
        default:
            printf("[CTRCARD] Unrecognized write8 $%08X: $%02X\n", addr, value);
    }
}

void Cartridge::write32_ctr(uint32_t addr, uint32_t value)
{
    if (addr >= 0x10004020 && addr < 0x10004030)
    {
        printf("[CTRCARD] Write cmd $%08X: $%08X\n", addr, value);

        value = bswp32(value);
        int index = 12 - (addr & 0xF);
        *(uint32_t*)&cmd_buffer[index] = value;
        return;
    }
    switch (addr)
    {
        case 0x10004000:
            printf("[CTRCARD] Write32 ROMCTRL: $%08X\n", value);
            ctr_romctrl.write_mode = value & (1 << 29);
            ctr_romctrl.irq_enable = value & (1 << 30);

            if (!ctr_romctrl.busy && value & (1 << 31))
            {
                ctr_romctrl.busy = true;
                process_ctr_cmd();
            }
            break;
        case 0x10004004:
            printf("[CTRCARD] BLKCNT: $%08X\n", value);
            read_block_count = (value & 0xFFFF) + 1;
            break;
        case 0x10004008:
            ctr_secctrl = value;
            break;
        case 0x10004030:
            printf("[CTRCARD] Write Card2: $%08X\n", value);
            if (card2_active)
            {
                *(uint32_t*)&data_buffer[data_pos] = value;
                data_pos += 4;
                data_bytes_left -= 4;
                if (data_pos == 0x200)
                {
                    card.write((char*)data_buffer, 0x200);
                    data_pos = 0;
                }
                if (data_bytes_left <= 0)
                    card2_active = false;
            }
            break;
        default:
            printf("[CTRCARD] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}

void Cartridge::write32_spicard(uint32_t addr, uint32_t value)
{
    if (!card_inserted())
        return;
    switch (addr)
    {
        case 0x1000D800:
            printf("[SPICARD] Write32 NSPI_CNT: $%08X\n", value);
            if (value & (1 << 15))
            {
                spi_input_pos = 0;
                spi_output_pos = 0;
                process_spicard_cmd();
            }
            break;
        case 0x1000D804:
            printf("[SPICARD] Clear chip select\n");

            switch (spi_state)
            {
                case SPICARD_STATE::WRITE_READY:
                    for (int i = 0; i < spi_block_len; i++)
                        save_data[spi_save_addr + i] = spi_input_buffer[i];
                    save_dirty = true;
                    break;
                case SPICARD_STATE::PROGRAM_READY:
                    for (int i = 0; i < spi_block_len; i++)
                        save_data[spi_save_addr + i] &= spi_input_buffer[i];
                    save_dirty = true;
                    break;
                default:
                    //Do nothing
                    break;
            }
            spi_state = SPICARD_STATE::IDLE;
            break;
        case 0x1000D808:
            printf("[SPICARD] Block len: $%08X\n", value);
            spi_block_len = value;
            break;
        case 0x1000D80C:
            //printf("[SPICARD] Write32 NSPI_FIFO: $%08X\n", value);
            *(uint32_t*)&spi_input_buffer[spi_input_pos] = value;
            spi_input_pos += 4;
            if (spi_input_pos >= sizeof(spi_input_buffer))
                EmuException::die("[SPICARD] Input pos exceeds size of input buffer!");
            break;
        default:
            printf("[SPICARD] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}
