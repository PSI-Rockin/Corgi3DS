#ifndef EMULATOR_HPP
#define EMULATOR_HPP
#include <cstdint>
#include "aes.hpp"
#include "arm.hpp"
#include "cp15.hpp"
#include "dma9.hpp"
#include "emmc.hpp"
#include "gpu.hpp"
#include "interrupt9.hpp"
#include "mpcore_pmr.hpp"
#include "pxi.hpp"
#include "rsa.hpp"
#include "sha.hpp"
#include "timers.hpp"

class Emulator
{
    private:
        //ROMs
        uint8_t boot9[1024 * 64], boot11[1024 * 64], otp[256];

        uint8_t* arm9_RAM;
        uint8_t* axi_RAM;

        CP15 arm9_cp15, sys_cp15, app_cp15;
        AES aes;
        ARM_CPU arm9, arm11;
        DMA9 dma9;
        EMMC emmc;
        GPU gpu;
        Interrupt9 int9;
        MPCore_PMR mpcore_pmr;
        PXI pxi;
        RSA rsa;
        SHA sha;
        Timers timers;

        uint8_t sysprot9, sysprot11;
    public:
        Emulator();
        ~Emulator();

        void reset();
        void run();

        void load_roms(uint8_t* boot9, uint8_t* boot11, uint8_t* otp);
        bool mount_nand(std::string file_name);

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

        uint8_t* get_buffer();
};

#endif // EMULATOR_HPP
