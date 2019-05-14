#ifndef EMULATOR_HPP
#define EMULATOR_HPP
#include <cstdint>
#include "arm9/aes.hpp"
#include "arm9/dma9.hpp"
#include "arm9/emmc.hpp"
#include "arm9/interrupt9.hpp"
#include "arm9/rsa.hpp"
#include "arm9/sha.hpp"

#include "arm11/gpu.hpp"
#include "arm11/mpcore_pmr.hpp"

#include "cpu/arm.hpp"
#include "cpu/cp15.hpp"

#include "i2c.hpp"
#include "pxi.hpp"
#include "timers.hpp"

class Emulator
{
    private:
        //ROMs
        uint8_t* boot9;
        uint8_t* boot11;
        uint8_t* otp;
        uint8_t otp_free[256], otp_locked[256];
        uint8_t boot9_free[1024 * 64], boot11_free[1024 * 64];
        uint8_t boot9_locked[1024 * 64], boot11_locked[1024 * 64];

        uint8_t twl_consoleid[8];

        uint8_t* arm9_RAM;
        uint8_t* axi_RAM;
        uint8_t* fcram;

        ARM_CPU arm9, arm11;
        CP15 arm9_cp15, app_cp15, sys_cp15;
        AES aes;
        DMA9 dma9;
        EMMC emmc;
        GPU gpu;
        I2C i2c;
        Interrupt9 int9;
        MPCore_PMR mpcore_pmr;
        PXI pxi;
        RSA rsa;
        SHA sha;
        Timers timers;

        uint32_t config_bootenv;

        uint16_t HID_PAD;

        uint8_t sysprot9, sysprot11;
    public:
        Emulator();
        ~Emulator();

        void reset(bool cold_boot = true);
        void run();
        void print_state();
        void dump();

        void load_roms(uint8_t* boot9, uint8_t* boot11, uint8_t* otp, uint8_t* cid);
        bool mount_nand(std::string file_name);
        bool mount_sd(std::string file_name);

        uint8_t arm9_read8(uint32_t addr);
        uint16_t arm9_read16(uint32_t addr);
        uint32_t arm9_read32(uint32_t addr);
        void arm9_write8(uint32_t addr, uint8_t value);
        void arm9_write16(uint32_t addr, uint16_t value);
        void arm9_write32(uint32_t addr, uint32_t value);

        uint8_t arm11_read8(uint32_t addr);
        uint16_t arm11_read16(uint32_t addr);
        uint32_t arm11_read32(uint32_t addr);
        void arm11_write8(uint32_t addr, uint8_t value);
        void arm11_write16(uint32_t addr, uint16_t value);
        void arm11_write32(uint32_t addr, uint32_t value);

        uint8_t* get_top_buffer();
        uint8_t* get_bottom_buffer();
        void set_pad(uint16_t pad);
};

#endif // EMULATOR_HPP
