#ifndef I2C_HPP
#define I2C_HPP
#include <cstdint>

struct I2C_CNT_REG
{
    bool device_selected;
    bool read_dir;
    bool ack_flag;
    bool irq_enable;
    bool busy;
    bool start;
    bool stop;

    uint8_t cur_device;
};

struct I2C_Device
{
    uint8_t cur_reg;
    bool reg_selected;
};

class MPCore_PMR;
class Scheduler;

class I2C
{
    private:
        MPCore_PMR* pmr;
        Scheduler* scheduler;
        I2C_CNT_REG cnt[3];
        I2C_Device devices[3][0x100];
        uint8_t data[3];

        uint8_t mcu_time[7];

        int mcu_counter;
        int mcu_7f_pos;
        uint8_t mcu_7f_buf[0x13];

        uint8_t mcu_int_pending[4];
        uint8_t mcu_int_mask[4];

        int get_id(uint32_t addr);

        uint8_t get_cnt(int id);
        void set_cnt(int id, uint8_t value);

        uint8_t read_device(int id, uint8_t device);
        void write_device(int id, uint8_t device, uint8_t value);

        uint8_t read_camera(int device, uint8_t reg_id);

        uint8_t read_mcu(uint8_t reg_id);
        void write_mcu(uint8_t reg_id, uint8_t value);
    public:
        I2C(MPCore_PMR* pmr, Scheduler* scheduler);

        void reset();
        void update_time();

        void do_transfer(int id);
        void mcu_interrupt(int id);

        uint8_t read8(uint32_t addr);
        void write8(uint32_t addr, uint8_t value);
};

#endif // I2C_HPP
