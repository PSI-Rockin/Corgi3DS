#include <cstdio>
#include <cstring>
#include "common/common.hpp"
#include "emulator.hpp"

Emulator::Emulator() :
    arm9(this, 9, &arm9_cp15, nullptr),
    appcore(this, 11, &app_cp15, &app_vfp),
    syscore(this, 12, &sys_cp15, &sys_vfp),
    arm9_cp15(9, &arm9, &arm9_pu),
    app_cp15(0, &appcore, &app_mmu),
    sys_cp15(1, &syscore, &sys_mmu),
    aes(&dma9, &int9),
    cartridge(&dma9, &int9),
    dma9(this, &int9, &scheduler),
    emmc(&int9, &dma9),
    gpu(&scheduler, &mpcore_pmr),
    i2c(&mpcore_pmr, &scheduler),
    int9(&arm9),
    mpcore_pmr(&appcore, &syscore, &timers),
    pxi(&mpcore_pmr, &int9),
    rsa(&int9),
    sha(&dma9),
    timers(&int9, &mpcore_pmr, this)
{
    arm9_RAM = nullptr;
    axi_RAM = nullptr;
    fcram = nullptr;
    dsp_mem = nullptr;
    vram = nullptr;
}

Emulator::~Emulator()
{
    delete[] arm9_RAM;
    delete[] axi_RAM;
    delete[] fcram;
    delete[] dsp_mem;
    delete[] vram;
}

void Emulator::reset(bool cold_boot)
{
    if (!arm9_RAM)
        arm9_RAM = new uint8_t[1024 * 1024];
    if (!axi_RAM)
        axi_RAM = new uint8_t[1024 * 512];
    if (!fcram)
        fcram = new uint8_t[1024 * 1024 * 128];
    if (!dsp_mem)
        dsp_mem = new uint8_t[1024 * 512];
    if (!vram)
        vram = new uint8_t[1024 * 1024 * 6];

    mpcore_pmr.reset();

    otp = otp_free;

    HID_PAD = 0xFFF;

    dsp.reset();
    gpu.reset(vram);
    i2c.reset();

    aes.reset();
    cartridge.reset();
    dma9.reset();
    emmc.reset();
    pxi.reset();

    sysprot9 = 0;
    sysprot11 = 0;

    if (cold_boot)
        config_bootenv = 0;

    config_cardctrl2 = 0;
    card_reset = 0;

    arm9_pu.reset();
    app_mmu.reset();
    sys_mmu.reset();

    arm9_pu.add_physical_mapping(arm9_RAM, 0x08000000, 1024 * 1024);
    arm9_pu.add_physical_mapping(dsp_mem, 0x1FF00000, 1024 * 512);
    arm9_pu.add_physical_mapping(axi_RAM, 0x1FF80000, 1024 * 512);
    arm9_pu.add_physical_mapping(fcram, 0x20000000, 1024 * 1024 * 128);
    arm9_pu.add_physical_mapping(boot9_free, 0xFFFF0000, 1024 * 64);
    arm9_pu.add_physical_mapping(vram, 0x18000000, 1024 * 1024 * 6);

    app_mmu.add_physical_mapping(boot11_free, 0, 1024 * 64);
    app_mmu.add_physical_mapping(boot11_free, 0x10000, 1024 * 64);
    app_mmu.add_physical_mapping(dsp_mem, 0x1FF00000, 1024 * 512);
    app_mmu.add_physical_mapping(axi_RAM, 0x1FF80000, 1024 * 512);
    app_mmu.add_physical_mapping(fcram, 0x20000000, 1024 * 1024 * 128);
    app_mmu.add_physical_mapping(vram, 0x18000000, 1024 * 1024 * 6);

    sys_mmu.add_physical_mapping(boot11_free, 0, 1024 * 64);
    sys_mmu.add_physical_mapping(boot11_free, 0x10000, 1024 * 64);
    sys_mmu.add_physical_mapping(dsp_mem, 0x1FF00000, 1024 * 512);
    sys_mmu.add_physical_mapping(axi_RAM, 0x1FF80000, 1024 * 512);
    sys_mmu.add_physical_mapping(fcram, 0x20000000, 1024 * 1024 * 128);
    sys_mmu.add_physical_mapping(vram, 0x18000000, 1024 * 1024 * 6);

    //We must reset the CPUs after the MMUs are initialized so we can get the TLB pointer
    arm9.reset();
    appcore.reset();
    syscore.reset();

    memset(ARM_CPU::global_exclusive_start, 0, sizeof(ARM_CPU::global_exclusive_start));
    memset(ARM_CPU::global_exclusive_end, 0, sizeof(ARM_CPU::global_exclusive_end));
    arm9_cp15.reset(true);
    sys_cp15.reset(false);
    app_cp15.reset(false);

    scheduler.reset();
}

void Emulator::run()
{
    static int frames = 0;
    i2c.update_time();
    printf("FRAME %d\n", frames);
    int cycles = 0;
    while (cycles < 4000000)
    {
        scheduler.calculate_cycles_to_run();
        int cycles11 = scheduler.get_cycles11_to_run();
        int cycles9 = scheduler.get_cycles9_to_run();
        cycles += cycles11;
        appcore.run(cycles11);
        syscore.run(cycles11);
        arm9.run(cycles9);
        timers.run(cycles11);
        dsp.run(cycles9);
        dma9.process_ndma_reqs();
        dma9.run_xdma();
        scheduler.process_events();
    }
    //VBLANK
    mpcore_pmr.assert_hw_irq(0x2A);
    frames++;
    gpu.render_frame();
}

void Emulator::print_state()
{
    printf("--PRINTING STATE--\n");
    printf("ARM9 state\n");
    arm9.print_state();

    printf("\nAppcore state\n");
    appcore.print_state();

    printf("\nSyscore state\n");
    syscore.print_state();

    printf("\n--END LOG--\n");
}

void Emulator::dump()
{
    std::ofstream hey("memdump.bin", std::ofstream::binary);
    hey.write((char*)arm9_RAM, 1024 * 1024);
    hey.close();
    EmuException::die("memdump");
}

void Emulator::memdump11(int id, uint64_t start, uint64_t size)
{
    ARM_CPU* core = nullptr;
    if (id == 11)
        core = &appcore;
    else
        core = &syscore;

    uint8_t* buffer = new uint8_t[size];

    for (uint64_t i = 0; i < size; i += 4)
    {
        *(uint32_t*)&buffer[i] = core->read32(start + i);

    }

    std::ofstream file("ld_so.bin", std::ofstream::binary);
    file.write((char*)buffer, size);
    file.close();
    EmuException::die("memdump11");
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

bool Emulator::mount_cartridge(std::string file_name)
{
    return cartridge.mount(file_name);
}

void Emulator::load_and_run_elf(uint8_t *elf, uint64_t size)
{
    reset();

    arm9_pu.add_physical_mapping(fcram, 0, 1024 * 1024 * 128);

    uint32_t e_entry = *(uint32_t*)&elf[0x18];
    uint32_t e_phoff = *(uint32_t*)&elf[0x1C];
    uint32_t e_shoff = *(uint32_t*)&elf[0x20];
    uint16_t e_phnum = *(uint16_t*)&elf[0x2C];
    uint16_t e_shnum = *(uint16_t*)&elf[0x30];
    uint16_t e_shstrndx = *(uint16_t*)&elf[0x32];

    printf("Entry: $%08X\n", e_entry);
    printf("Program header start: $%08X\n", e_phoff);
    printf("Section header start: $%08X\n", e_shoff);
    printf("Program header entries: %d\n", e_phnum);
    printf("Section header entries: %d\n", e_shnum);
    printf("Section header names index: %d\n", e_shstrndx);

    for (unsigned int i = e_phoff; i < e_phoff + (e_phnum * 0x20); i += 0x20)
    {
        uint32_t p_offset = *(uint32_t*)&elf[i + 0x4];
        uint32_t p_paddr = *(uint32_t*)&elf[i + 0xC];
        uint32_t p_filesz = *(uint32_t*)&elf[i + 0x10];
        uint32_t p_memsz = *(uint32_t*)&elf[i + 0x14];
        printf("\nProgram header\n");
        printf("p_type: $%08X\n", *(uint32_t*)&elf[i]);
        printf("p_offset: $%08X\n", p_offset);
        printf("p_vaddr: $%08X\n", *(uint32_t*)&elf[i + 0x8]);
        printf("p_paddr: $%08X\n", p_paddr);
        printf("p_filesz: $%08X\n", p_filesz);
        printf("p_memsz: $%08X\n", p_memsz);

        int mem_w = p_paddr;
        for (unsigned int file_w = p_offset; file_w < (p_offset + p_filesz); file_w += 4)
        {
            uint32_t word = *(uint32_t*)&elf[file_w];
            *(uint32_t*)&fcram[mem_w] = word;
            mem_w += 4;
        }
    }

    arm9.jp(e_entry, true);

    //Switch to system mode
    arm9.cps((1 << 17) | 0x1F);
    arm9.set_register(REG_SP, e_entry);
    //arm9.set_disassembly(true);
    arm9.run(1024 * 1024);
}

uint8_t Emulator::arm9_read8(uint32_t addr)
{
    if (addr >= 0x08000000 && addr < 0x08100000)
        return arm9_RAM[addr & 0xFFFFF];

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
            return config_cardselect;
        case 0x10000010:
        {
            uint8_t reg = config_cardctrl2;

            if (!cartridge.card_inserted())
                reg |= 1;

            if (card_reset)
            {
                card_reset--;
                if (!card_reset)
                    config_cardctrl2 = 0;
            }

            //printf("[ARM9] Read8 CARD_CFG2: $%02X\n", reg);
            return reg;
        }
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
    if (addr >= 0x10003000 && addr < 0x10004000)
        return timers.arm9_read16(addr);

    if (addr >= 0x10006000 && addr < 0x10007000)
        return emmc.read16(addr);

    //PRNG - TODO
    if (addr >= 0x10011000 && addr < 0x10012000)
    {
        printf("[ARM9] Read16 PRNG\n");
        return addr & 0xFFFF;
    }

    if (addr >= 0x10160000 && addr < 0x10161000)
    {
        printf("[SPI2] Unrecognized read16 $%08X\n", addr);
        return 0;
    }

    if (addr >= 0x10164000 && addr < 0x10165000)
        return cartridge.read16_ntr(addr);

    switch (addr)
    {
        case 0x10000004:
            return 0; //debug control?
        case 0x1000000C:
            return config_cardselect; //card select
        case 0x10000020:
            return 0;
        case 0x10000FFC:
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
    if (addr >= 0x08000000 && addr < 0x08100000)
        return *(uint32_t*)&arm9_RAM[addr & 0xFFFFF];

    if (addr >= 0x18000000 && addr < 0x18600000)
        return gpu.read_vram<uint32_t>(addr);

    if (addr >= 0x10002000 && addr < 0x10003000)
        return dma9.read32_ndma(addr);

    if (addr >= 0x10004000 && addr < 0x10006000)
        return cartridge.read32_ctr(addr);

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
        return cartridge.read32_spicard(addr);

    if (addr >= 0x10011000 && addr < 0x10012000)
    {
        printf("[ARM9] Unrecognized read32 from PRNG $%08X\n", addr);
        return 0;
    }

    if (addr >= 0x10012000 && addr < 0x10012100)
        return *(uint32_t*)&otp[addr & 0xFF];

    if (addr >= 0x10164000 && addr < 0x10165000)
        return cartridge.read32_ntr(addr);

    if (addr >= 0x1FF80000 && addr < 0x20000000)
        return *(uint32_t*)&axi_RAM[addr & 0x7FFFF];

    switch (addr)
    {
        case 0x10000FFC:
            return 0;
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
    if (addr >= 0x10160000 && addr < 0x10161000)
    {
        printf("[SPI2] Unrecognized write8 $%08X: $%02X\n", addr, value);
        return;
    }
    if (addr >= 0x10164000 && addr < 0x10165000)
    {
        cartridge.write8_ntr(addr, value);
        return;
    }
    if (addr >= 0x10004000 && addr < 0x10005000)
    {
        cartridge.write8_ctr(addr, value);
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
            {
                arm9_pu.remove_physical_mapping(0xFFFF0000, 1024 * 64);
                arm9_pu.add_physical_mapping(boot9_locked, 0xFFFF0000, 1024 * 64);
            }

            //Disable access to OTP
            if (value & 0x2)
                otp = otp_locked;

            sysprot9 = value;
            return;
        case 0x10000001:
            if (value & 0x1)
            {
                app_mmu.remove_physical_mapping(0, 1024 * 64);
                app_mmu.remove_physical_mapping(0x10000, 1024 * 64);
                sys_mmu.remove_physical_mapping(0, 1024 * 64);
                sys_mmu.remove_physical_mapping(0x10000, 1024 * 64);

                app_mmu.add_physical_mapping(boot11_locked, 0, 1024 * 64);
                app_mmu.add_physical_mapping(boot11_locked, 0x10000, 1024 * 64);
                sys_mmu.add_physical_mapping(boot11_locked, 0, 1024 * 64);
                sys_mmu.add_physical_mapping(boot11_locked, 0x10000, 1024 * 64);
            }

            sysprot11 = value;
            return;
        case 0x10000002:
            return;
        case 0x10000008:
            return;
        case 0x10000010:
            printf("[ARM9] Write8 CARD_CFG2: $%02X\n", value);
            config_cardctrl2 = value;
            if ((value & 0x0C) == 0x0C)
                card_reset = 10;
            return;
        case 0x10000200:
            return; //supposed to control how much FCRAM N3DS has - ignore because we do O3DS for now
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
    if (addr >= 0x10160000 && addr < 0x10161000)
    {
        printf("[SPI2] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10161000 && addr < 0x10162000)
    {
        printf("[A9 I2C] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10164000 && addr < 0x10165000)
    {
        cartridge.write16_ntr(addr, value);
        return;
    }
    switch (addr)
    {
        case 0x10000004:
            return;
        case 0x1000000C:
            printf("[CFG9] Write cardselect: $%04X\n", value);
            config_cardselect = value;
            return;
        case 0x10000012:
            return;
        case 0x10000014:
            return;
        case 0x10000020:
            return;
        case 0x10008004:
            pxi.write_cnt9(value);
            return;
        case 0x10009004:
            aes.write_mac_count(value);
            return;
        case 0x10009006:
            aes.write_block_count(value);
            return;
    }
    EmuException::die("[ARM9] Invalid write16 $%08X: $%04X\n", addr, value);
}

void Emulator::arm9_write32(uint32_t addr, uint32_t value)
{
    if (addr >= 0x10003000 && addr < 0x10004000)
    {
        timers.arm9_write16(addr, value & 0xFFFF);
        timers.arm9_write16(addr + 2, value >> 16);
        return;
    }
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
    if (addr >= 0x10002000 && addr < 0x10003000)
    {
        dma9.write32_ndma(addr, value);
        return;
    }
    if (addr >= 0x10004000 && addr < 0x10006000)
    {
        cartridge.write32_ctr(addr, value);
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
        cartridge.write32_spicard(addr, value);
        return;
    }

    if (addr >= 0x10012100 && addr < 0x10012108)
    {
        printf("TWL consoleid $%08X: $%08X\n", addr, value);
        *(uint32_t*)&twl_consoleid[addr & 0x7] = value;
        return;
    }

    if (addr >= 0x10164000 && addr < 0x10165000)
    {
        cartridge.write32_ntr(addr, value);
        return;
    }

    if (addr >= 0x1FF00000 && addr < 0x1FF80000)
    {
        *(uint32_t*)&dsp_mem[addr & 0x7FFFF] = value;
        return;
    }

    if (addr >= 0x20000000 && addr < 0x28000000)
    {
        *(uint32_t*)&fcram[addr & 0x07FFFFFF] = value;
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
        case 0x10008004:
            pxi.write_cnt9(value & 0xFFFF);
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

uint8_t Emulator::arm11_read8(int core, uint32_t addr)
{
    if (addr >= 0x10140000 && addr < 0x10140010)
        return dsp_mem_config[addr & 0xF];
    if (addr >= 0x10144000 && addr < 0x10145000)
        return i2c.read8(addr);
    if (addr >= 0x10147000 && addr < 0x10148000)
    {
        printf("[GPIO] Unrecognized read8 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10148000 && addr < 0x10149000)
        return i2c.read8(addr);
    if (addr >= 0x10161000 && addr < 0x10162000)
        return i2c.read8(addr);
    if (addr >= 0x17E00000 && addr < 0x17E02000)
        return mpcore_pmr.read8(core, addr);
    switch (addr)
    {
        case 0x1014010C:
            return 0;
        case 0x10140180:
            return 0; //WiFi power
        case 0x10141204:
            return 1; //GPU power
        case 0x10141208:
            return 0; //Unk GPU power reg
        case 0x10141220:
            return 0; //Enable FCRAM?
        case 0x10141224:
            return 0; //Camera power
        case 0x10141230:
            return 0; //DSP power
        case 0x10163000:
        case 0x10163001:
        case 0x10163002:
        case 0x10163003:
            return (pxi.read_sync11() >> ((addr & 0x3) * 8)) & 0xFF;
    }
    EmuException::die("[ARM11] Invalid read8 $%08X\n", addr);
    return 0;
}

uint16_t Emulator::arm11_read16(int core, uint32_t addr)
{
    if (addr >= 0x10103000 && addr < 0x10104000)
    {
        printf("[CSND] Unrecognized read16 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10120000 && addr < 0x10122000)
    {
        printf("[Y2R] Unrecognized read16 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10122000 && addr < 0x10123000)
    {
        //These addresses are read a bunch of times, which causes slowdown as WiFi is stubbed
        if (addr == 0x1012201C || addr == 0x1012202C)
            return 0;
        printf("[WIFI] Unrecognized read16 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10145000 && addr < 0x10146000)
    {
        printf("[CODEC] Unrecognized read16 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10147000 && addr < 0x10148000)
    {
        printf("[GPIO] Unrecognized read16 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10161000 && addr < 0x10162000)
    {
        printf("[I2C] Unrecognized read8 $%08X\n", addr);
        return 0;
    }
    /*if (addr >= 0x10203000 && addr < 0x10204000)
        return dsp.read16(addr);*/
    switch (addr)
    {
        case 0x101401C0:
            return 0x7; //3DS/DS SPI switch
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

uint32_t Emulator::arm11_read32(int core, uint32_t addr)
{
    if (addr >= 0x17E00000 && addr < 0x17E02000)
        return mpcore_pmr.read32(core, addr);
    if (addr >= 0x10103000 && addr < 0x10104000)
    {
        printf("[CSND] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10120000 && addr < 0x10122000)
    {
        printf("[Y2R] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10200000 && addr < 0x10201000)
    {
        printf("[CDMA] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10400000 && addr < 0x10402000)
        return gpu.read32(addr);
    if (addr >= 0x18000000 && addr < 0x18600000)
        return gpu.read_vram<uint32_t>(addr);
    if (addr >= 0x10101000 && addr < 0x10102000)
        return hash.read32(addr);
    if (addr >= 0x10202000 && addr < 0x10203000)
    {
        printf("[LCD] Unrecognized read $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10142000 && addr < 0x10144000)
    {
        printf("[SPI] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10147000 && addr < 0x10148000)
    {
        printf("[GPIO] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x10160000 && addr < 0x10161000)
    {
        printf("[SPI] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    if (addr >= 0x1020F000 && addr < 0x10210000)
    {
        printf("[AXI] Unrecognized read32 $%08X\n", addr);
        return 0;
    }
    switch (addr)
    {
        case 0x10141200:
            return 0; //GPU power config
        case 0x10146000:
            return HID_PAD;
        case 0x10163000:
            return pxi.read_sync11();
        case 0x10163008:
            //3dslinux reads from SEND11 for some mysterious reason...
            return 0;
        case 0x1016300C:
            return pxi.read_msg11();
    }
    EmuException::die("[ARM11] Invalid read32 $%08X\n", addr);
    return 0;
}

void Emulator::arm11_write8(int core, uint32_t addr, uint8_t value)
{
    if (addr >= 0x17E00000 && addr < 0x17E02000)
    {
        mpcore_pmr.write8(core, addr, value);
        return;
    }

    if (addr >= 0x10140000 && addr < 0x10140010)
    {
        dsp_mem_config[addr & 0xF] = value;
        return;
    }

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

    if (addr >= 0x10148000 && addr < 0x10149000)
    {
        i2c.write8(addr, value);
        return;
    }

    switch (addr)
    {
        case 0x10140104:
            return;
        case 0x1014010C:
            return;
        case 0x10140180:
            return;
        case 0x10141204:
            return;
        case 0x10141208:
            return;
        case 0x10141220:
            return;
        case 0x10141224:
            return;
        case 0x10141230:
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

void Emulator::arm11_write16(int core, uint32_t addr, uint16_t value)
{
    if (addr >= 0x17E00000 && addr < 0x17E02000)
    {
        mpcore_pmr.write16(core, addr, value);
        return;
    }
    if (addr >= 0x10103000 && addr < 0x10104000)
    {
        printf("[CSND] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10120000 && addr < 0x10122000)
    {
        printf("[Y2R] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10122000 && addr < 0x10123000)
    {
        printf("[WIFI] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10144000 && addr < 0x10145000)
    {
        printf("[I2C] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10145000 && addr < 0x10146000)
    {
        printf("[CODEC] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10147000 && addr < 0x10148000)
    {
        printf("[GPIO] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10161000 && addr < 0x10162000)
    {
        printf("[I2C] Unrecognized write16 $%08X: $%04X\n", addr, value);
        return;
    }
    if (addr >= 0x10203000 && addr < 0x10204000)
    {
        dsp.write16(addr, value);
        return;
    }
    switch (addr)
    {
        case 0x101401C0:
            return;
        case 0x10148002:
            return;
        case 0x10148004:
            return;
        case 0x10163004:
            pxi.write_cnt11(value);
            return;
    }
    EmuException::die("[ARM11] Invalid write16 $%08X: $%04X\n", addr, value);
}

void Emulator::arm11_write32(int core, uint32_t addr, uint32_t value)
{
    if (addr >= 0x17E00000 && addr < 0x17E02000)
    {
        mpcore_pmr.write32(core, addr, value);
        return;
    }
    if (addr >= 0x10101000 && addr < 0x10102000)
    {
        hash.write32(addr, value);
        return;
    }
    if (addr >= 0x10103000 && addr < 0x10104000)
    {
        printf("[CSND] Unrecognized write32 $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x10120000 && addr < 0x10122000)
    {
        printf("[Y2R] Unrecognized write32 $%08X: $%08X\n", addr, value);
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
    if (addr >= 0x1020F000 && addr < 0x10210000)
    {
        printf("[AXI] Unrecognized write32 $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x10301000 && addr < 0x10302000)
    {
        hash.write_fifo(value);
        return;
    }
    if (addr >= 0x10400000 && addr < 0x10402000)
    {
        gpu.write32(addr, value);
        return;
    }
    if (addr >= 0x10142000 && addr < 0x10144000)
    {
        printf("[SPI] Unrecognized write32 $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x10147000 && addr < 0x10148000)
    {
        printf("[GPIO] Unrecognized write32 $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x10160000 && addr < 0x10161000)
    {
        printf("[SPI] Unrecognized write32 $%08X: $%08X\n", addr, value);
        return;
    }
    if (addr >= 0x18000000 && addr < 0x18600000)
    {
        gpu.write_vram<uint32_t>(addr, value);
        return;
    }
    switch (addr)
    {
        case 0x10140140:
            return; //GPUPROT
        case 0x10141200:
            return;
        case 0x10163000:
            pxi.write_sync11(value);
            return;
        case 0x10163004:
            pxi.write_cnt11(value & 0xFFFF);
            return;
        case 0x10163008:
            //if (value == 0x10a9b8)
                //arm9.set_disassembly(true);
            pxi.send_to_9(value);
            return;
        case 0x10202014:
            return;
    }
    EmuException::die("[ARM11] Invalid write32 $%08X: $%08X\n", addr, value);
}

void Emulator::arm11_send_events(int id)
{
    appcore.send_event(id);
    syscore.send_event(id);
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
