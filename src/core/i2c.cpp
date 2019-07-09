#include <cstring>
#include <cstdio>
#include <ctime>
#include "arm11/mpcore_pmr.hpp"
#include "common/common.hpp"
#include "i2c.hpp"
#include "scheduler.hpp"
#include "emulator.hpp"

#define itob(i) ((i)/10*16 + (i)%10)    /* u_char to BCD */

I2C::I2C(MPCore_PMR* pmr, Scheduler* scheduler) : pmr(pmr), scheduler(scheduler)
{

}

void I2C::reset()
{
    memset(cnt, 0, sizeof(cnt));
    memset(devices, 0, sizeof(devices));
    mcu_counter = 0;
}

uint8_t I2C::read8(uint32_t addr)
{
    int id = get_id(addr);
    addr &= 0xFFF;

    uint8_t reg = 0;
    switch (addr)
    {
        case 0x000:
            reg = data[id];
            break;
        case 0x001:
            reg = get_cnt(id);
            break;
        default:
            printf("[I2C%d] Unrecognized read8 $%08X\n", id, addr);
    }
    return reg;
}

void I2C::write8(uint32_t addr, uint8_t value)
{
    int id = get_id(addr);
    addr &= 0xFFF;

    switch (addr)
    {
        case 0x000:
            printf("[I2C%d] Write data: $%02X\n", id, value);
            data[id] = value;
            break;
        case 0x001:
            set_cnt(id, value);
            break;
        default:
            printf("[I2C%d] Unrecognized write8 $%08X: $%02X\n", id, addr, value);
    }
}

int I2C::get_id(uint32_t addr)
{
    if (addr >= 0x10144000 && addr < 0x10145000)
        return 1;
    if (addr >= 0x10148000 && addr < 0x10149000)
        return 2;
    return 0;
}

uint8_t I2C::get_cnt(int id)
{
    uint8_t reg = 0;
    reg |= cnt[id].stop;
    reg |= cnt[id].start << 1;
    reg |= cnt[id].ack_flag << 4;
    reg |= cnt[id].read_dir << 5;
    reg |= cnt[id].irq_enable << 6;
    reg |= cnt[id].busy << 7;
    return reg;
}

void I2C::set_cnt(int id, uint8_t value)
{
    printf("[I2C%d] Set CNT: $%02X\n", id, value);
    cnt[id].stop = value & 0x1;
    cnt[id].start = value & 0x2;
    cnt[id].ack_flag &= ~((value & (1 << 4)) != 0);
    cnt[id].read_dir = value & (1 << 5);
    cnt[id].irq_enable = value & (1 << 6);

    if (value & (1 << 7))
    {
        cnt[id].ack_flag = true;
        cnt[id].busy = true;
        scheduler->add_event(I2C_TRANSFER, &Emulator::i2c_transfer_event, 20000, id);
    }
}

void I2C::do_transfer(int id)
{
    cnt[id].busy = false;
    if (cnt[id].start)
    {
        uint8_t dev = data[id] & ~0x1;
        if (!cnt[id].device_selected)
        {
            devices[id][dev].reg_selected = false;
            printf("[I2C%d] Selecting device $%02X\n", id, dev);
        }
        cnt[id].device_selected = true;
        cnt[id].cur_device = dev;
    }
    else
    {
        uint8_t cur_dev = cnt[id].cur_device;
        if (!devices[id][cur_dev].reg_selected)
        {
            devices[id][cur_dev].reg_selected = true;
            devices[id][cur_dev].cur_reg = data[id];

            printf("[I2C%d] Selecting device $%02X reg $%02X\n", id, cur_dev, data[id]);
        }
        else
        {
            if (cnt[id].read_dir)
                data[id] = read_device(id, cur_dev);
            else
                write_device(id, cur_dev, data[id]);

            devices[id][cur_dev].cur_reg++;
        }
        if (cnt[id].stop)
        {
            devices[id][cur_dev].reg_selected = false;
            cnt[id].device_selected = false;
        }
    }

    if (cnt[id].irq_enable)
    {
        switch (id)
        {
            case 0:
                pmr->assert_hw_irq(0x54);
                break;
            case 1:
                pmr->assert_hw_irq(0x55);
                break;
            case 2:
                pmr->assert_hw_irq(0x5C);
                break;
        }
    }
}

void I2C::update_time()
{
    time_t raw_time;
    struct tm * time;
    std::time(&raw_time);
    time = std::gmtime(&raw_time);

    mcu_time[0] = itob(time->tm_sec);
    mcu_time[1] = itob(time->tm_min);
    mcu_time[2] = itob(time->tm_hour);
    mcu_time[4] = itob(time->tm_mday);
    mcu_time[5] = itob(time->tm_mon + 1); //Jan = 0

    uint16_t year = time->tm_year - 100;
    mcu_time[6] = itob(year & 0xFF);
}

uint8_t I2C::read_device(int id, uint8_t device)
{
    uint16_t dev = device | (id << 8);

    uint8_t reg = 0;
    switch (dev)
    {
        case 0x14A:
            return read_mcu(devices[id][device].cur_reg);
        default:
            printf("[I2C%d] Unrecognized read device $%02X\n", id, device);
    }
    return reg;
}

void I2C::write_device(int id, uint8_t device, uint8_t value)
{
    uint16_t dev = device | (id << 8);
    switch (dev)
    {
        case 0x14A:
            write_mcu(devices[id][device].cur_reg, value);
            break;
        default:
            printf("[I2C%d] Unrecognized write device $%02X ($%02X)\n", id, device, value);
    }
}

uint8_t I2C::read_mcu(uint8_t reg_id)
{
    if (reg_id >= 0x30 && reg_id < 0x37)
    {
        printf("Read time: $%02X\n", mcu_time[reg_id - 0x30]);
        return mcu_time[reg_id - 0x30];
    }

    switch (reg_id)
    {
        case 0x00:
            return 0x04; //Version high
        case 0x01:
            return 0x59; //Version low
        case 0x0B:
            return 0xFF; //battery percent
        case 0x0F:
            return 0x02; //bit 1 = shell state
        default:
            printf("[I2C_MCU] Unrecognized read register $%02X\n", reg_id);
    }
    return 0;
}

void I2C::write_mcu(uint8_t reg_id, uint8_t value)
{
    if (mcu_counter)
    {
        mcu_counter--;
        return;
    }
    switch (reg_id)
    {
        case 0x07:
            //The registers 05, 06, and 07 are used to unlock the MCU's FIRM so that it can be written to.
            //A FIRM is 0x4000 bytes, so we have to ignore that many writes.
            mcu_counter = 0x4000;
            break;
        case 0x20:
            //Poweroff
            if (value & 0x1)
                EmuException::die("[I2C_MCU] Powering off!");

            //Reboot
            if (value & 0x4)
                EmuException::reboot();
            break;
        default:
            printf("[I2C_MCU] Unrecognized write register $%02X\n", reg_id);
    }
}
