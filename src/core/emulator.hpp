#ifndef EMULATOR_HPP
#define EMULATOR_HPP
#include <cstdint>
#include "arm9/aes.hpp"
#include "arm9/cartridge.hpp"
#include "arm9/dma9.hpp"
#include "arm9/emmc.hpp"
#include "arm9/interrupt9.hpp"
#include "arm9/rsa.hpp"
#include "arm9/sha.hpp"

#include "arm11/dsp.hpp"
#include "arm11/gpu.hpp"
#include "arm11/hash.hpp"
#include "arm11/mpcore_pmr.hpp"
#include "arm11/wifi.hpp"

#include "cpu/arm.hpp"
#include "cpu/cp15.hpp"
#include "cpu/mmu.hpp"
#include "cpu/vfp.hpp"

#include "i2c.hpp"
#include "pxi.hpp"
#include "scheduler.hpp"
#include "spi.hpp"
#include "timers.hpp"

class Emulator
{
    private:
        //ROMs
        uint8_t* otp;
        uint8_t otp_free[256], otp_locked[256];
        uint8_t boot9_free[1024 * 64], boot11_free[1024 * 64];
        uint8_t boot9_locked[1024 * 64], boot11_locked[1024 * 64];

        bool is_n3ds;
        int core_count;
        uint32_t clock_ctrl;
        uint32_t boot_overlay_addr;
        uint8_t boot_ctrl[4];

        uint32_t fcram_size;
        uint32_t arm9_ram_size;
        uint32_t qtm_size;

        uint8_t twl_consoleid[8];

        uint8_t* arm9_RAM;
        uint8_t* axi_RAM;
        uint8_t* fcram;
        uint8_t* dsp_mem;
        uint8_t* vram;
        uint8_t* qtm_ram;

        ARM_CPU arm9, arm11[4];
        CP15 arm9_cp15, arm11_cp15[4];
        MMU arm9_pu, arm11_mmu[4];
        VFP vfp[4];

        AES aes;
        Cartridge cartridge;
        Corelink_DMA cdma;
        DMA9 dma9;
        DSP dsp;
        EMMC emmc;
        GPU gpu;
        HASH hash;
        I2C i2c;
        Interrupt9 int9;
        MPCore_PMR mpcore_pmr;
        PXI pxi;
        RSA rsa;
        Scheduler scheduler;
        SHA sha;
        SPI spi;
        Timers timers;
        WiFi wifi;

        uint32_t config_bootenv;
        uint16_t config_cardselect;

        uint8_t config_cardctrl2;
        int card_reset;

        uint8_t dsp_mem_config[16];

        uint16_t HID_PAD;

        uint8_t sysprot9, sysprot11;
    public:
        Emulator();
        ~Emulator();

        void reset(bool cold_boot = true);
        void run();
        void print_state();
        void dump();
        void memdump11(int id, uint64_t start, uint64_t size);

        void load_roms(uint8_t* boot9, uint8_t* boot11);
        bool parse_essentials();
        bool mount_nand(std::string file_name);
        bool mount_sd(std::string file_name);
        bool mount_cartridge(std::string file_name);

        void load_and_run_elf(uint8_t* elf, uint64_t size);

        uint8_t arm9_read8(uint32_t addr);
        uint16_t arm9_read16(uint32_t addr);
        uint32_t arm9_read32(uint32_t addr);
        void arm9_write8(uint32_t addr, uint8_t value);
        void arm9_write16(uint32_t addr, uint16_t value);
        void arm9_write32(uint32_t addr, uint32_t value);

        uint8_t arm11_read8(int core, uint32_t addr);
        uint16_t arm11_read16(int core, uint32_t addr);
        uint32_t arm11_read32(int core, uint32_t addr);
        void arm11_write8(int core, uint32_t addr, uint8_t value);
        void arm11_write16(int core, uint32_t addr, uint16_t value);
        void arm11_write32(int core, uint32_t addr, uint32_t value);

        void arm11_send_events(int id);

        uint8_t* get_top_buffer();
        uint8_t* get_bottom_buffer();
        void set_pad(uint16_t pad);

        void set_touchscreen(uint16_t x, uint16_t y);
        void clear_touchscreen();
};

#endif // EMULATOR_HPP
