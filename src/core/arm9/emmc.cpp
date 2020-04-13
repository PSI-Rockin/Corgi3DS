#include <cstdio>
#include <cstring>
#include "../common/common.hpp"
#include "dma9.hpp"
#include "emmc.hpp"
#include "interrupt9.hpp"

#define ISTAT_CMDEND 0x1
#define ISTAT_DATAEND 0x4
#define ISTAT_RXRDY 0x01000000
#define ISTAT_TXRQ 0x02000000

EMMC::EMMC(Interrupt9* int9, DMA9* dma9) : int9(int9), dma9(dma9)
{
    regcsd[0] = 0xe9964040;
    regcsd[1] = 0xdff6db7f;
    regcsd[2] = 0x2a0f5901;
    regcsd[3] = 0x3f269001;

    sd_cid[0] = 0xD71C65CD;
    sd_cid[1] = 0x4445147B;
    sd_cid[2] = 0x4D324731;
    sd_cid[3] = 0x00150100;
}

EMMC::~EMMC()
{
    if (nand.is_open())
        nand.close();

    if (sd.is_open())
        sd.close();
}

void EMMC::reset()
{
    istat = 0;
    imask = 0;
    app_command = false;
    ocr_reg = 0x80FF8080;
    transfer_size = 0;
    transfer_pos = 0;
    transfer_blocks = 0;
    transfer_buffer = nullptr;
    block_transfer = false;
    state = MMC_Idle;

    sd_data32.rd32rdy_irq_pending = false;
    sd_data32.tx32rq_irq_pending = false;
    sd_write_protected = false;

    memset(regsd_status, 0, 64);
    memset(regscr, 0, 8);

    *(uint32_t*)&regscr[1] = 0x012a0000;
}

bool EMMC::mount_nand(std::string file_name)
{
    nand.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    return nand.is_open();
}

bool EMMC::mount_sd(std::string file_name)
{
    sd.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    return sd.is_open();
}

bool EMMC::parse_essentials(uint8_t *otp)
{
    //Look at offset 0x200 on the NAND and try to load the OTP and CID off essentials.exefs

    char essentials[0x200];
    nand.seekg(0x200);
    nand.read((char*)essentials, 0x200);

    bool otp_found = false, cid_found = false;

    int counter = 0;
    while (!(otp_found && cid_found) && counter < 0x200)
    {
        uint32_t offs = *(uint32_t*)&essentials[counter + 8];

        //Account for the size of the ExeFS header and its position on the NAND
        offs += 0x400;
        if (!strncmp(essentials + counter, "otp", 8))
        {
            nand.seekg(offs);
            nand.read((char*)otp, 256);
            otp_found = true;
        }
        if (!strncmp(essentials + counter, "nand_cid", 8))
        {
            nand.seekg(offs);
            nand.read((char*)nand_cid, 16);
            cid_found = true;
        }
        counter += 0x10;
    }

    if (otp_found && cid_found)
        return true;

    return false;
}

bool EMMC::is_n3ds()
{
    //Read the partition crypt types at 0x118-0x120. If one of them is 0x3, this is a New3DS NAND.
    char sector[0x200];
    nand.seekg(0);
    nand.read((char*)sector, 0x200);

    for (int i = 0x118; i < 0x120; i++)
    {
        if (sector[i] == 0x3)
            return true;
    }
    return false;
}

uint16_t EMMC::read16(uint32_t addr)
{
    uint16_t reg = 0;
    if (addr >= 0x1000600C && addr < 0x1000601C)
    {
        int index = ((addr - 0x1000600C) / 4) & 0x3;
        if (addr % 4 == 2)
            reg = response[index] >> 16;
        else
            reg = response[index] & 0xFFFF;
        //printf("[EMMC] Read response $%08X: $%04X\n", addr, reg);
        return reg;
    }
    switch (addr)
    {
        case 0x10006002:
            reg = port_select;
            break;
        case 0x1000600A:
            reg = data_blocks;
            break;
        case 0x1000601C:
            reg = (istat & 0xFFFF);
            reg |= 1 << 5;
            reg |= (!sd_write_protected) << 7;
            //printf("[EMMC] Read ISTAT_L: $%04X\n", reg);
            break;
        case 0x1000601E:
            reg = istat >> 16;
            //printf("[EMMC] Read ISTAT_H: $%04X\n", reg);
            break;
        case 0x10006020:
            reg = imask & 0xFFFF;
            //printf("[EMMC] Read IMASK_L: $%04X\n", reg);
            break;
        case 0x10006022:
            reg = imask >> 16;
            //printf("[EMMC] Read IMASK_H: $%04X\n", reg);
            break;
        case 0x10006026:
            reg = data_block_len;
            break;
        case 0x10006030:
            reg = read_fifo();
            break;
        case 0x100060D8:
            reg = ctrl;
            break;
        case 0x100060F8:
            //MMC is inserted
            reg = 0x4;
            break;
        case 0x10006100:
            reg |= sd_data32.data32 << 1;
            reg |= sd_data32.rd32rdy_irq_pending << 8;
            reg |= sd_data32.tx32rq_irq_pending << 9;
            reg |= sd_data32.rd32rdy_irq_enable << 11;
            reg |= sd_data32.tx32rq_irq_enable << 12;
            //printf("[EMMC] Read SD_DATA32_IRQ: $%04X\n", reg);
            break;
        case 0x10006104:
            reg = data32_block_len;
            break;
        default:
            printf("[EMMC] Unrecognized read16 $%08X\n", addr);
            break;
    }
    return reg;
}

uint32_t EMMC::read32(uint32_t addr)
{
    uint32_t reg = 0;
    switch (addr)
    {
        case 0x1000601C:
            reg = istat;
            reg |= 1 << 5;
            reg |= (!sd_write_protected) << 7;
            break;
        case 0x1000610C:
            reg = read_fifo32();
            break;
        default:
            EmuException::die("[EMMC] Unrecognized read32 $%08X\n", addr);
            return 0;
    }
    return reg;
}

void EMMC::write16(uint32_t addr, uint16_t value)
{
    switch (addr)
    {
        case 0x10006000:
            printf("[EMMC] Send command, arg: $%08X\n", argument);
            if (app_command)
                send_acmd(value & 0x3F);
            else
                send_cmd(value & 0x3F);
            break;
        case 0x10006002:
            printf("[EMMC] Port select: $%04X\n", value);
            port_select = value;
            break;
        case 0x10006004:
            argument &= ~0xFFFF;
            argument |= value;
            //printf("[EMMC] Set arg lo: $%04X\n", value);
            break;
        case 0x10006006:
            argument &= 0xFFFF;
            argument |= value << 16;
            //printf("[EMMC] Set arg hi: $%04X ($%08X)\n", value, argument);
            break;
        case 0x1000600A:
            data_blocks = value;
            //printf("[EMMC] Set BLKCOUNT: $%04X\n", value);
            break;
        case 0x1000601C:
            //printf("[EMMC] Write ISTAT_L: $%04X\n", value);
            istat &= value | 0xFFFF0000;
            break;
        case 0x1000601E:
            //printf("[EMMC] Write ISTAT_H: $%04X\n", value);
            istat &= (value << 16) | 0xFFFF;
            break;
        case 0x10006020:
            //printf("[EMMC] Set IMASK_L: $%04X\n", value);
            imask &= ~0xFFFF;
            imask |= value;
            break;
        case 0x10006022:
            //printf("[EMMC] Set IMASK_H: $%04X\n", value);
            imask &= 0xFFFF;
            imask |= value << 16;
            break;
        case 0x10006026:
            //printf("[EMMC] Set BLKLEN: $%04X\n", value);
            data_block_len = value;
            if (data_block_len > 0x200)
                data_block_len = 0x200;
            break;
        case 0x100060D8:
            ctrl = value;
            break;
        case 0x10006100:
            //printf("[EMMC] Write SD_DATA32_IRQ: $%04X\n", value);
            sd_data32.data32 = value & (1 << 1);

            sd_data32.rd32rdy_irq_enable = value & (1 << 11);
            sd_data32.tx32rq_irq_enable = value & (1 << 12);
            break;
        case 0x10006104:
            //printf("[EMMC] Write SD_DATA32_BLKLEN: $%04X\n", value);
            data32_block_len = value & 0x3FF;
            break;
        case 0x10006108:
            //printf("[EMMC] Write SD_DATA32_BLKCOUNT: $%04X\n", value);
            data32_blocks = value;
            break;
        default:
            printf("[EMMC] Unrecognized write16 $%08X: $%04X\n", addr, value);
            break;
    }
}

void EMMC::write32(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        case 0x1000610C:
            write_fifo32(value);
            return;
        default:
            write16(addr, value & 0xFFFF);
            write16(addr + 2, value >> 16);
            //EmuException::die("[EMMC] Unrecognized write32 $%08X: $%08X\n", addr, value);
    }
}

void EMMC::send_cmd(int command)
{
    printf("[EMMC] CMD%d\n", command);
    switch (command)
    {
        case 0:
            //Reset
            istat = 0;
            response[0] = get_csr();
            command_end();
            state = MMC_Idle;
            break;
        case 1:
            response[0] = ocr_reg;
            command_end();
            break;
        case 2:
            if (nand_selected())
                memcpy(response, nand_cid, 16);
            else
                memcpy(response, sd_cid, 16);
            command_end();
            if (state == MMC_Ready)
                state = MMC_Identify;
            break;
        case 3:
            //3dmoo9 sets the "regrca" register to 1 no matter what...
            response[0] = 0x10000 | get_r1_reply();
            command_end();
            if (state == MMC_Identify)
                state = MMC_Standby;
            break;
        case 6:
            response[0] = get_r1_reply();
            command_end();

            if (state == MMC_Transfer)
                state = MMC_Program;
            break;
        case 7:
            response[0] = get_r1_reply();
            command_end();
            break;
        case 8:
            response[0] = 0x1AA;
            command_end();
            break;
        case 9:
            memcpy(response, regcsd, 16);
            command_end();
            break;
        case 10:
            memcpy(response, nand_cid, 16);
            command_end();
            break;
        case 12:
            response[0] = get_r1_reply();
            transfer_size = 0;
            transfer_end();
            switch (state)
            {
                case MMC_Data:
                case MMC_Receive:
                    state = MMC_Transfer;
                    break;
                case MMC_Transfer:
                    state = MMC_Standby;
                    break;
                default:
                    break;
            }
            break;
        case 13:
            response[0] = get_r1_reply();
            command_end();
            break;
        case 16:
            cmd_block_len = argument;
            command_end();
            break;
        case 18:
            transfer_start_addr = argument;
            state = MMC_Transfer;
            response[0] = get_r1_reply();
            state = MMC_Data;
            transfer_pos = 0;
            transfer_blocks = data_blocks;
            transfer_size = data_block_len;
            block_transfer = true;

            if (nand_selected())
                cur_transfer_drive = &nand;
            else
            {
                cur_transfer_drive = &sd;
                transfer_start_addr *= data_block_len;
            }
            printf("[EMMC] Read multiple blocks (start: $%lX blocks: $%08X)\n", transfer_start_addr, data_blocks);
            printf("Reading from %s\n", (nand_selected()) ? "NAND" : "SD");

            if (cur_transfer_drive->eof())
                cur_transfer_drive->clear();

            cur_transfer_drive->seekg(transfer_start_addr, std::ios::beg);
            cur_transfer_drive->read((char*)&nand_block, transfer_size);

            transfer_buffer = (uint8_t*)nand_block;
            data_ready();
            //command_end();
            break;
        case 25:
            transfer_start_addr = argument;
            state = MMC_Transfer;
            response[0] = get_r1_reply();
            state = MMC_Receive;

            transfer_pos = 0;
            transfer_blocks = data_blocks;
            transfer_size = data_block_len;
            block_transfer = true;

            if (nand_selected())
                cur_transfer_drive = &nand;
            else
            {
                cur_transfer_drive = &sd;
                transfer_start_addr *= data_block_len;
            }
            printf("[EMMC] Write multiple blocks (start: $%lX blocks: $%08X)\n", transfer_start_addr, data_blocks);

            if (argument >= 0x0DD80000 && argument < 0x0DD80000 + 0x64C00)
            {
                printf("Write to title.db: $%08X\n", argument - 0x0DD80000);
            }

            if (cur_transfer_drive->eof())
                cur_transfer_drive->clear();

            cur_transfer_drive->seekp(transfer_start_addr, std::ios::beg);
            transfer_buffer = (uint8_t*)nand_block;

            write_ready();
            //command_end();
            break;
        case 55:
            app_command = true;
            response[0] = get_r1_reply();
            command_end();
            break;
        default:
            EmuException::die("[EMMC] Unrecognized CMD%d\n", command);
    }
}

void EMMC::send_acmd(int command)
{
    printf("[EMMC] ACMD%d\n", command);

    istat &= ~0x1;

    app_command = false;

    switch (command)
    {
        case 6:
            *(uint32_t*)&regsd_status[60] = ((*(uint32_t*)&regsd_status[60] & ~3) << 30) | argument << 30;
            response[0] = get_sdr1_reply();
            command_end();
            break;
        case 13:
            response[0] = get_sdr1_reply();
            sd_data32.rd32rdy_irq_pending = true;
            set_istat(ISTAT_RXRDY);

            transfer_buffer = (uint8_t*)regsd_status;
            transfer_pos = 0;
            transfer_size = sizeof(regsd_status);
            command_end();
            break;
        case 41:
            //Bit 30 of the OCR indicates SDXC, meaning that the eMMC can handle addresses larger than 2 GB
            if (nand_selected())
                response[0] = ocr_reg;
            else
                response[0] = ocr_reg | (1 << 30);
            command_end();
            if (state == MMC_Idle)
                state = MMC_Ready;
            break;
        case 42:
            response[0] = get_sdr1_reply();
            command_end();
            break;
        case 51:
            response[0] = get_sdr1_reply();
            sd_data32.rd32rdy_irq_pending = true;
            //if (sd_data32.rd32rdy_irq_enable)
                set_istat(ISTAT_RXRDY);

            transfer_buffer = (uint8_t*)regscr;
            transfer_size = sizeof(regscr);
            transfer_pos = 0;
            command_end();
            data_ready();
            break;
        default:
            EmuException::die("[EMMC] Unrecognized ACMD%d\n", command);
    }
}

uint32_t EMMC::get_csr()
{
    //Indicates card is ready
    return 1 << 9;
}

uint32_t EMMC::get_r1_reply()
{
    return get_sdr1_reply();
}

uint32_t EMMC::get_sdr1_reply()
{
    uint32_t reg = app_command << 5;
    reg |= state << 9;
    if (!transfer_size)
        reg |= 1 << 8; //ready for data
    printf("R1: $%08X State: %d\n", reg, reg >> 9);
    return reg;
}

void EMMC::command_end()
{
    set_istat(ISTAT_CMDEND);
}

void EMMC::data_ready()
{
    sd_data32.tx32rq_irq_pending = false;
    sd_data32.rd32rdy_irq_pending = true;

    if (sd_data32.rd32rdy_irq_enable)
        set_istat(ISTAT_RXRDY);
    dma9->set_ndma_req(NDMA_AES2);
}

void EMMC::write_ready()
{
    sd_data32.rd32rdy_irq_pending = false;
    sd_data32.tx32rq_irq_pending = false;

    if (sd_data32.tx32rq_irq_enable)
        set_istat(ISTAT_TXRQ);
}

void EMMC::set_istat(uint32_t field)
{
    uint32_t old_istat = istat;
    istat |= field;

    printf("ISTAT: $%08X IMSK: $%08X COMB: $%08X\n", istat, imask, istat & imask);

    if (!(old_istat & imask & field) && (istat & imask & field))
        int9->assert_irq(16);
}

uint16_t EMMC::read_fifo()
{
    if (transfer_size)
    {
        uint16_t value = *(uint16_t*)&transfer_buffer[transfer_pos];
        printf("[EMMC] Read FIFO16: $%04X\n", value);
        transfer_pos += 2;
        transfer_size -= 2;

        if (!transfer_size)
        {
            //data_ready();
            transfer_end();
        }
        return value;
    }
    return 0;
}

uint32_t EMMC::read_fifo32()
{
    if (transfer_size)
    {
        uint32_t value = *(uint32_t*)&transfer_buffer[transfer_pos];
        //printf("[EMMC] Read FIFO32: $%08X ($%08X)\n", value, transfer_pos);
        transfer_pos += 4;
        transfer_size -= 4;

        if (!transfer_size)
        {
            data_ready();
            transfer_pos = 0;
            if (block_transfer)
            {
                transfer_blocks--;
                //printf("Transfer blocks: %d\n", transfer_blocks);
                if (!transfer_blocks)
                    transfer_end();
                else
                {
                    dma9->try_ndma_transfer(NDMA_MMC1);
                    transfer_size = data_block_len;
                    cur_transfer_drive->read((char*)transfer_buffer, transfer_size);
                }
            }
            else
                transfer_end();
        }
        return value;
    }
    return 0;
}

void EMMC::write_fifo32(uint32_t value)
{
    if (transfer_size)
    {
        *(uint32_t*)&transfer_buffer[transfer_pos] = value;
        printf("[EMMC] Write FIFO32: $%08X\n", value);
        transfer_pos += 4;
        transfer_size -= 4;

        if (!transfer_size)
        {
            transfer_pos = 0;
            cur_transfer_drive->write((char*)transfer_buffer, data_block_len);
            if (block_transfer)
            {
                transfer_blocks--;
                if (!transfer_blocks)
                {
                    transfer_end();
                    cur_transfer_drive->flush();
                }
                else
                {
                    dma9->try_ndma_transfer(NDMA_MMC1);
                    transfer_size = data_block_len;
                    write_ready();
                }
            }
            else
            {
                transfer_end();
                cur_transfer_drive->flush();
            }
        }
    }
}

void EMMC::transfer_end()
{
    transfer_buffer = nullptr;
    block_transfer = false;
    sd_data32.rd32rdy_irq_pending = false;
    printf("[EMMC] Transfer end\n");
    switch (state)
    {
        case MMC_Data:
            state = MMC_Transfer;
            break;
        case MMC_Receive:
            state = MMC_Transfer;
            break;
        case MMC_Transfer:
            state = MMC_Standby;
            break;
        default:
            state = MMC_Standby;
            break;
    }
    //clear busy bit to allow "command end" IRQ to hit
    istat &= ~0x1;
    set_istat(ISTAT_DATAEND);
    command_end();
    dma9->clear_ndma_req(NDMA_MMC1);
    dma9->clear_ndma_req(NDMA_AES2);
}
