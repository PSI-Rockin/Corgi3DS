#include <cstdio>
#include <cstring>
#include "common/common.hpp"
#include "emulator.hpp"

Emulator::Emulator() :
    arm9(this, 9, &arm9_cp15),
    arm11(this, 11, &app_cp15),
    arm9_cp15(0, &arm9),
    app_cp15(0, &arm11),
    sys_cp15(1, &arm11),
    aes(&dma9, &int9),
    dma9(this, &int9),
    emmc(&int9, &dma9),
    int9(&arm9),
    mpcore_pmr(&arm11),
    pxi(&mpcore_pmr, &int9),
    rsa(&int9),
    sha(&dma9),
    timers(&int9)
{
    arm9_RAM = nullptr;
    axi_RAM = nullptr;
    fcram = nullptr;
}

Emulator::~Emulator()
{
    delete[] arm9_RAM;
    delete[] axi_RAM;
    delete[] fcram;
}

void Emulator::reset(bool cold_boot)
{
    if (!arm9_RAM)
        arm9_RAM = new uint8_t[1024 * 1024];
    if (!axi_RAM)
        axi_RAM = new uint8_t[1024 * 512];
    if (!fcram)
        fcram = new uint8_t[1024 * 1024 * 128];
    arm9.reset();
    arm11.reset();
    arm9_cp15.reset(true);
    sys_cp15.reset(false);
    app_cp15.reset(false);
    mpcore_pmr.reset();

    boot9 = boot9_free;
    boot11 = boot11_free;
    otp = otp_free;

    HID_PAD = 0xFFF;

    gpu.reset();
    i2c.reset();

    aes.reset();
    dma9.reset();
    emmc.reset();
    pxi.reset();

    sysprot9 = 0;
    sysprot11 = 0;

    if (cold_boot)
        config_bootenv = 0;
}

void Emulator::run()
{
    i2c.update_time();
    if (arm9.is_halted() && (int9.read_ie() & 0x4000) && !pxi.get_hle())
    {
        pxi.send_to_9(0x7);
        //pxi.send_to_9(0x10040);
        //pxi.send_to_9(0x0);
        pxi.activate_hle();
        //arm9.set_disassembly(true);
    }
    for (int i = 0; i < 50000; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            arm9.run();
            arm11.run();
            timers.run();
        }
        dma9.process_ndma_reqs();
        dma9.run_xdma();
    }
    gpu.render_frame();
}

void Emulator::print_state()
{
    printf("--PRINTING STATE--\n");
    printf("ARM9 state\n");
    arm9.print_state();

    printf("\nARM11 state\n");
    arm11.print_state();

    printf("\n--END LOG--\n");
}

void Emulator::dump()
{
    std::ofstream hey("memdump.bin", std::ofstream::binary);
    hey.write((char*)arm9_RAM, 1024 * 1024);
    hey.close();
    EmuException::die("memdump");
}

void Emulator::load_roms(uint8_t *boot9, uint8_t *boot11, uint8_t *otp, uint8_t* cid)
{
    //The boot ROMs lock the upper halves of themselves (0x8000 and up)
    //This is required for emulation. Boot11 at least checks for the ROM to be disabled before booting a FIRM.
    memset(boot9_locked, 0, 1024 * 64);
    memset(boot11_locked, 0, 1024 * 64);

    memcpy(boot9_free, boot9, 1024 * 64);
    memcpy(boot11_free, boot11, 1024 * 64);

    memcpy(boot9_locked, boot9, 1024 * 32);
    memcpy(boot11_locked, boot11, 1024 * 32);

    memcpy(otp_free, otp, 256);
    memset(otp_locked, 0xFF, 256);
    emmc.load_cid(cid);
}

bool Emulator::mount_nand(std::string file_name)
{
    return emmc.mount_nand(file_name);
}

bool Emulator::mount_sd(std::string file_name)
{
    return emmc.mount_sd(file_name);
}

uint8_t Emulator::arm9_read8(uint32_t addr)
{
    if (addr >= 0xFFFF0000)
        return boot9[addr & 0xFFFF];

    if (addr >= 0x08000000 && addr < 0x08100000)
        return arm9_RAM[addr & 0xFFFFF];

    if (addr >= 0x20000000 && addr < 0x28000000)
        return fcram[addr & 0x07FFFFFF];

    if (addr >= 0x18000000 && addr < 0x18600000)
        return gpu.read_vram<uint8_t>(addr);

    if (addr >= 0x1000A040 && addr < 0x1000A080)
        return sha.read_hash(addr);

    if (addr >= 0x1000B000 && addr < 0x1000C000)
        return rsa.read8(addr);

    if (addr >= 0x10144000 && addr < 0x10145000)
        return i2c.read8(addr);

    if (addr >= 0x10148000 && addr < 0x10149000)
        return i2c.read8(addr);

    if (addr >= 0x10160000 && addr < 0x10161000)
    {
        printf("[SPI2] Unrecognized read8 $%08X\n", addr);
        return 0;
    }

    if (addr >= 0x10161000 && addr < 0x10162000)
        return i2c.read8(addr);

    if (addr >= 0x18000000 && addr < 0x18600000)
        return gpu.read_vram<uint8_t>(addr);

    if (addr >= 0x1FF80000 && addr < 0x20000000)
        return axi_RAM[addr & 0x7FFFF];

    switch (addr)
    {
        case 0x10000000:
            return sysprot9;
        case 0x10000001:
            return sysprot11;
        case 0x10000002:
            return 0; //Related to powering on ARM11?
        case 0x10000008:
            return 0; //AES related
        case 0x1000000C:
            return 0; //card select
        case 0x10000010:
            return 1; //Cartridge not inserted
        case 0x10000200:
            return 0; //New3DS memory hidden
        case 0x10008000:
            return pxi.read_sync9() & 0xFF;
        case 0x10008003:
            return pxi.read_sync9() >> 24;
        case 0x10009011:
            return aes.read_keycnt();
        case 0x10010010:
            return 0; //0=retail, 1=dev
        case 0x10010014:
            return 0;
        case 0x10141200:
            return 1;
    }

    EmuException::die("[ARM9] Invalid read8 $%08X\n", addr);
    return 0;
}

uint16_t Emulator::arm9_read16(uint32_t addr)
{
    if (addr >= 0xFFFF0000)
        return *(uint16_t*)&boot9[addr & 0xFFFF];

    if (addr >= 0x08000000 && addr < 0x08100000)
        return *(uint16_t*)&arm9_RAM[addr & 0xFFFFF];

    if (addr >= 0x20000000 && addr < 0x28000000)
        return *(uint16_t*)&fcram[addr & 0x07FFFFFF];

    if (addr >= 0x10003000 && addr < 0x10004000)
        return timers.arm9_read16(addr);

    if (addr >= 0x10006000 && addr < 0x10007000)
        return emmc.read16(addr);

    if (addr >= 0x10160000 && addr < 0x10161000)
    {
        printf("[SPI2] Unrecognized read16 $%08X\n", addr);
        return 0;
    }

    switch (addr)
    {
        case 0x10000004:
            return 0; //debug control?
        case 0x1000000C:
            return 0; //card select
        case 0x10000020:
            return 0;
        case 0x10008004:
            return pxi.read_cnt9();
        case 0x10146000:
            return HID_PAD; //bits on = keys not pressed
    }

    EmuException::die("[ARM9] Invalid read16 $%08X\n", addr);
    return 0;
}

uint32_t Emulator::arm9_read32(uint32_t addr)
{
    if (addr >= 0xFFFF0000)
        return *(uint32_t*)&boot9[addr & 0xFFFF];

    if (addr >= 0x08000000 && addr < 0x08100000)
        return *(uint32_t*)&arm9_RAM[addr & 0xFFFFF];

    if (addr >= 0x18000000 && addr < 0x18600000)
        return gpu.read_vram<uint32_t>(addr);

    if (addr >= 0x20000000 && addr < 0x28000000)
        return *(uint32_t*)&fcram[addr & 0x07FFFFFF];

    if (addr >= 0x10002000 && addr < 0x10003000)
        return dma9.read32_ndma(addr);

    if (addr >= 0x10004000 && addr < 0x10006000)
    {
        printf("[ARM9] Unrecognized read32 from CTRCARD $%08X\n", addr);
        return 0;
    }

    if (addr >= 0x10006000 && addr < 0x10007000)
        return emmc.read32(addr);

    if (addr >= 0x10009000 && addr < 0x1000A000)
        return aes.read32(addr);

    if (addr >= 0x1000A000 && addr < 0x1000B000)
        return sha.read32(addr);

    if (addr >= 0x1000B000 && addr < 0x1000C000)
        return rsa.read32(addr);

    if (addr >= 0x1000C000 && addr < 0x1000D000)
        return dma9.read32_xdma(addr);

    if (addr >= 0x1000D800 && addr < 0x1000E000)
    {
        printf("[ARM9] Unrecognized read32 from SPICARD $%08X\n", addr);
        return 0;
    }

    if (addr >= 0x10012000 && addr < 0x10012100)
        return *(uint32_t*)&otp[addr & 0xFF];

    if (addr >= 0x10164000 && addr < 0x10165000)
    {
        printf("[ARM9] Unrecognized read32 from NTRCARD $%08X\n", addr);
        return 0;
    }

    if (addr >= 0x1FF80000 && addr < 0x20000000)
        return *(uint32_t*)&axi_RAM[addr & 0x7FFFF];

    switch (addr)
    {
        case 0x10001000:
            return int9.read_ie();
        case 0x10001004:
            return int9.read_if();
        case 0x10008000:
            return pxi.read_sync9();
        case 0x1000800C:
            return pxi.read_msg9();
        case 0x10010000:
            return config_bootenv;
        case 0x101401C0:
            return 0; //SPI control
        case 0x10140FFC:
            return 0x1; //bit 1 = New3DS (we're only emulating Old3DS for now)
        case 0x10146000:
            return HID_PAD;
    }

    EmuException::die("[ARM9] Invalid read32 $%08X\n", addr);
    return 0;
}

void Emulator::arm9_write8(uint32_t addr, uint8_t value)
{
    if (addr >= 0x08000000 && addr < 0x08100000)
    {
        arm9_RAM[addr & 0xFFFFF] = value;
        return;
    }
    if (addr >= 0x1FF80000 && addr < 0x20000000)
    {
        axi_RAM[addr & 0x7FFFF] = value;
        return;
    }
    if (addr >= 0x20000000 && addr < 0x28000000)
    {
        fcram[addr & 0x07FFFFFF] = value;
        return;
    }
    if (addr >= 0x18000000 && addr < 0x18600000)
    {
        gpu.write_vram<uint8_t>(addr, value);
        return;
    }
    if (addr >= 0x10144000 && addr < 0x10145000)
    {
        i2c.write8(addr, value);
        return;
    }
    if (addr >= 0x10160000 && addr < 0x10170000)
    {
        printf("[SPI2] Unrecognized write8 $%08X: $%02X\n", addr, value);
        return;
    }
    if (addr >= 0x1000B000 && addr < 0x1000C000)
    {
        rsa.write8(addr, value);
        return;
    }
    if (addr >= 0x10008000 && addr < 0x10008004)
    {
        uint32_t sync = pxi.read_sync9();
        int shift = (addr & 0x3) * 8;
        int mask = ~(0xFF << shift);
        sync &= mask;
        pxi.write_sync9(sync | (value << shift));
        return;
    }
    switch (addr)
    {
        case 0x10000000:
            //Disable access to sensitive parts of boot ROM
            if (value & 0x1)
                boot9 = boot9_locked;

            //Disable access to OTP
            if (value & 0x2)
                otp = otp_locked;

            sysprot9 = value;
            return;
        case 0x10000001:
            if (value & 0x1)
                boot11 = boot11_locked;

            sysprot11 = value;
            return;
        case 0x10000002:
            return;
        case 0x10000008:
            return;
        case 0x10000010:
            return;
        case 0x10009010:
            aes.write_keysel(value);
            return;
        case 0x10009011:
            aes.write_keycnt(value);
            return;
        case 0x10010014:
            return;
    }
    EmuException::die("[ARM9] Invalid write8 $%08X: $%02X\n", addr, value);
}

void Emulator::arm9_write16(uint32_t addr, uint16_t value)
{
    if (addr >= 0x08000000 && addr < 0x08100000)
    {
        *(uint16_t*)&arm9_RAM[addr & 0xFFFFF] = value;
        return;
    }
    if (addr >= 0x1FF80000 && addr < 0x20000000)
    {
        *(uint16_t*)&axi_RAM[addr & 0x7FFFF] = value;
        return;
    }
    if (addr >= 0x20000000 && addr < 0x28000000)
    {
        *(uint16_t*)&fcram[addr & 0x07FFFFFF] = value;
        return;
    }
    if (addr >= 0x10003000 && addr < 0x10004000)
    {
        timers.arm9_write16(addr, value);
        return;
    }
    if (addr >= 0x10006000 && addr < 0x10007000)
    {
        emmc.write16(addr, value);
        return;
    }
    if (addr >= 0x10144000 && addr < 0x10145000)
    {
        printf("[A9 I2C] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10148000 && addr < 0x10149000)
    {
        printf("[A9 I2C] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10160000 && addr < 0x10170000)
    {
        printf("[SPI2] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    switch (addr)
    {
        case 0x10000004:
            return;
        case 0x1000000C:
            return; //card select
        case 0x10000012:
            return;
        case 0x10000014:
            return;
        case 0x10000020:
            return;
        case 0x10008004:
            pxi.write_cnt9(value);
            return;
        case 0x10009006:
            aes.write_block_count(value);
            return;
    }
    EmuException::die("[ARM9] Invalid write16 $%08X: $%04X\n", addr, value);
}

void Emulator::arm9_write32(uint32_t addr, uint32_t value)
{
    if (addr >= 0x08000000 && addr < 0x08100000)
    {
        *(uint32_t*)&arm9_RAM[addr & 0xFFFFF] = value;
        return;
    }
    if (addr >= 0x1FF80000 && addr < 0x20000000)
    {
        *(uint32_t*)&axi_RAM[addr & 0x7FFFF] = value;
        return;
    }
    if (addr >= 0x18000000 && addr < 0x18600000)
    {
        gpu.write_vram<uint32_t>(addr, value);
        return;
    }
    if (addr >= 0x20000000 && addr < 0x28000000)
    {
        *(uint32_t*)&fcram[addr & 0x07FFFFFF] = value;
        return;
    }
    if (addr >= 0x10002000 && addr < 0x10003000)
    {
        dma9.write32_ndma(addr, value);
        return;
    }
    if (addr >= 0x10004000 && addr < 0x10006000)
    {
        printf("[ARM9] Unrecognized write32 to CTRCARD $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x10006000 && addr < 0x10007000)
    {
        emmc.write32(addr, value);
        return;
    }
    if (addr >= 0x10009000 && addr < 0x1000A000)
    {
        aes.write32(addr, value);
        return;
    }
    if (addr >= 0x1000A000 && addr < 0x1000B000)
    {
        sha.write32(addr, value);
        return;
    }
    if (addr >= 0x1000B000 && addr < 0x1000C000)
    {
        rsa.write32(addr, value);
        return;
    }
    if (addr >= 0x1000C000 && addr < 0x1000D000)
    {
        dma9.write32_xdma(addr, value);
        return;
    }
    if (addr >= 0x1000D800 && addr < 0x1000E000)
    {
        printf("[ARM9] Unrecognized write32 to SPICARD $%08X: $%08X\n", addr, value);
        return;
    }

    if (addr >= 0x10012100 && addr < 0x10012108)
    {
        *(uint32_t*)&twl_consoleid[addr & 0x7] = value;
        return;
    }

    if (addr >= 0x10164000 && addr < 0x10165000)
    {
        printf("[ARM9] Unrecognized write32 to NTRCARD $%08X: $%08X\n", addr, value);
        return;
    }

    //DSP memory
    if (addr >= 0x1FF00000 && addr < 0x1FF80000)
        return;

    //B9S writes here to cause a data abort during boot9 exec, which allows it to dump the boot ROMs.
    //Data aborts not implemented yet, so just ignore
    if (addr >= 0xC0000000 && addr < 0xC0000200)
    {
        return;
    }
    switch (addr)
    {
        case 0x10000020:
            printf("[ARM9] Set SDMMCCTL: $%08X\n", value);
            return;
        case 0x10001000:
            int9.write_ie(value);
            return;
        case 0x10001004:
            int9.write_if(value);
            return;
        case 0x10008000:
            pxi.write_sync9(value);
            return;
        case 0x10008008:
            pxi.send_to_11(value);
            return;
        case 0x10010000:
            config_bootenv = value;
            return;
    }
    EmuException::die("[ARM9] Invalid write32 $%08X: $%08X\n", addr, value);
}

uint8_t Emulator::arm11_read8(uint32_t addr)
{
    if (addr < 0x20000)
        return boot11[addr & 0xFFFF];
    if (addr >= 0x1FF80000 && addr < 0x20000000)
        return axi_RAM[addr & 0x7FFFF];
    if (addr >= 0x10144000 && addr < 0x10145000)
        return i2c.read8(addr);
    if (addr >= 0x10147000 && addr < 0x10148000)
    {
        printf("[GPIO] Unrecognized read8 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10161000 && addr < 0x10162000)
        return i2c.read8(addr);
    if (addr >= 0x17E00000 && addr < 0x17E02000)
        return mpcore_pmr.read8(addr);
    switch (addr)
    {
        case 0x10141204:
            return 1; //GPU power
        case 0x10141208:
            return 0; //Unk GPU power reg
        case 0x10141220:
            return 0; //Enable FCRAM?
        case 0x10163000:
            return pxi.read_sync11() & 0xFF;
    }
    EmuException::die("[ARM11] Invalid read8 $%08X\n", addr);
    return 0;
}

uint16_t Emulator::arm11_read16(uint32_t addr)
{
    if (addr < 0x20000)
        return *(uint16_t*)&boot11[addr & 0xFFFF];
    if (addr >= 0x1FF80000 && addr < 0x20000000)
        return *(uint16_t*)&axi_RAM[addr & 0x7FFFF];
    if (addr >= 0x10161000 && addr < 0x10162000)
    {
        printf("[I2C] Unrecognized read8 $%08X\n", addr);
        return 0;
    }
    switch (addr)
    {
        case 0x10140FFC:
            return 0x1; //Clock multiplier; bit 2 off = 2x
        case 0x10146000:
            return HID_PAD;
        case 0x10163004:
            return pxi.read_cnt11();
    }
    EmuException::die("[ARM11] Invalid read16 $%08X\n", addr);
    return 0;
}

uint32_t Emulator::arm11_read32(uint32_t addr)
{
    if (addr < 0x20000)
        return *(uint32_t*)&boot11[addr & 0xFFFF];
    if (addr >= 0x17E00000 && addr < 0x17E02000)
        return mpcore_pmr.read32(addr);
    if (addr >= 0x1FF80000 && addr < 0x20000000)
        return *(uint32_t*)&axi_RAM[addr & 0x7FFFF];
    if (addr >= 0x10200000 && addr < 0x10201000)
    {
        printf("[CDMA] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10400000 && addr < 0x10402000)
        return gpu.read32(addr);
    if (addr >= 0x18000000 && addr < 0x18600000)
        return gpu.read_vram<uint32_t>(addr);
    if (addr >= 0x10202000 && addr < 0x10203000)
    {
        printf("[LCD] Unrecognized read $%08X\n", addr);
        return 0;
    }
    switch (addr)
    {
        case 0x10141200:
            return 0; //GPU power config
        case 0x10163000:
            return pxi.read_sync11();
        case 0x1016300C:
            return pxi.read_msg11();
    }
    EmuException::die("[ARM11] Invalid read32 $%08X\n", addr);
    return 0;
}

void Emulator::arm11_write8(uint32_t addr, uint8_t value)
{
    if (addr >= 0x1FF80000 && addr < 0x20000000)
    {
        axi_RAM[addr & 0x7FFFF] = value;
        return;
    }
    if (addr >= 0x17E00000 && addr < 0x17E02000)
    {
        mpcore_pmr.write8(addr, value);
        return;
    }

    //Mapping data to DSP
    if (addr >= 0x10140000 && addr < 0x10140010)
        return;

    if (addr >= 0x10144000 && addr < 0x10145000)
    {
        i2c.write8(addr, value);
        return;
    }
    if (addr >= 0x10161000 && addr < 0x10162000)
    {
        i2c.write8(addr, value);
        return;
    }

    switch (addr)
    {
        case 0x10141204:
            return;
        case 0x10141208:
            return;
        case 0x10141220:
            return;
        case 0x10163001:
        {
            uint32_t sync = pxi.read_sync11() & 0xFFFF00FF;
            pxi.write_sync11(sync | (value << 8));
        }
            return;
        case 0x10163003:
        {
            uint32_t sync = pxi.read_sync11() & 0x00FFFFFF;
            pxi.write_sync11(sync | (value << 24));
        }
            return;
    }
    EmuException::die("[ARM11] Invalid write8 $%08X: $%02X\n", addr, value);
}

void Emulator::arm11_write16(uint32_t addr, uint16_t value)
{
    if (addr >= 0x1FF80000 && addr < 0x20000000)
    {
        *(uint16_t*)&axi_RAM[addr & 0x7FFFF] = value;
        return;
    }
    if (addr >= 0x17E00000 && addr < 0x17E02000)
    {
        mpcore_pmr.write16(addr, value);
        return;
    }
    if (addr >= 0x10144000 && addr < 0x10145000)
    {
        printf("[I2C] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10161000 && addr < 0x10162000)
    {
        printf("[I2C] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    switch (addr)
    {
        case 0x10163004:
            pxi.write_cnt11(value);
            return;
    }
    EmuException::die("[ARM11] Invalid write16 $%08X: $%04X\n", addr, value);
}

void Emulator::arm11_write32(uint32_t addr, uint32_t value)
{
    if (addr >= 0x1FF80000 && addr < 0x20000000)
    {
        *(uint32_t*)&axi_RAM[addr & 0x7FFFF] = value;
        return;
    }
    if (addr >= 0x17E00000 && addr < 0x17E02000)
    {
        mpcore_pmr.write32(addr, value);
        return;
    }
    if (addr >= 0x10200000 && addr < 0x10201000)
    {
        printf("[CDMA] Unrecognized write32 $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x10202000 && addr < 0x10203000)
    {
        printf("[LCD] Unrecognized write32 $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x10400000 && addr < 0x10402000)
    {
        gpu.write32(addr, value);
        return;
    }
    if (addr >= 0x18000000 && addr < 0x18600000)
    {
        gpu.write_vram<uint32_t>(addr, value);
        return;
    }
    switch (addr)
    {
        case 0x10141200:
            return;
        case 0x10163000:
            pxi.write_sync11(value);
            return;
        case 0x10202014:
            return;
    }
    EmuException::die("[ARM11] Invalid write32 $%08X: $%08X\n", addr, value);
}

uint8_t* Emulator::get_top_buffer()
{
    return gpu.get_top_buffer();
}

uint8_t* Emulator::get_bottom_buffer()
{
    return gpu.get_bottom_buffer();
}

void Emulator::set_pad(uint16_t pad)
{
    HID_PAD = ~pad;
    HID_PAD &= 0xFFF;
}
