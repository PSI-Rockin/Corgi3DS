#include <fstream>
#include <switch.h>

#include "../core/emulator.hpp"
#include "../core/common/exceptions.hpp"

using namespace std;

int main()
{
    uint8_t boot9_rom[1024 * 64], boot11_rom[1024 * 64];

    ifstream boot9("boot9.bin");
    if (!boot9.is_open())
    {
        printf("Failed to open boot9.bin\n");
        return 1;
    }

    boot9.read((char*)&boot9_rom, sizeof(boot9_rom));

    boot9.close();

    ifstream boot11("boot11.bin");
    if (!boot11.is_open())
    {
        printf("Failed to open boot11.bin\n");
        return 1;
    }

    boot11.read((char*)&boot11_rom, sizeof(boot11_rom));

    boot11.close();

    Emulator e;
    if (!e.mount_nand("nand.bin"))
    {
        printf("Failed to open nand.bin\n");
        return 1;
    }

    if (!e.mount_sd("sd.bin"))
    {
        printf("Failed to open sd.bin\n");
        return 1;
    }

    if (false) // blorp
    {
        if (!e.mount_cartridge("cart.3ds"))
        {
            printf("Failed to open cart.3ds\n");
            return 1;
        }
    }

    printf("All files loaded successfully!\n");
    e.load_roms(boot9_rom, boot11_rom);
    e.parse_essentials();
    if (!e.parse_essentials())
    {
        printf("Failed to find OTP and CID in essentials.exefs.\n");
        printf("Please make sure your NAND is dumped from the latest version of GodMode9.\n");
        return 1;
    }
    e.reset();

    // Overclock the Switch CPU
    ClkrstSession cpuSession;
    clkrstInitialize();
    clkrstOpenSession(&cpuSession, PcvModuleId_CpuBus, 0);
    clkrstSetClockRate(&cpuSession, 1785000000);

    // Create a framebuffer
    NWindow* win = nwindowGetDefault();
    Framebuffer fb;
    framebufferCreate(&fb, win, 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    appletLockExit();

    while (appletMainLoop())
    {
        try
        {
            e.run();

            // Draw the display
            u32 stride;
            u32 *framebuf = (u32*)framebufferBegin(&fb, &stride);
            uint32_t *topBuf = (uint32_t*)e.get_top_buffer();
            for (u32 y = 0; y < 240; y++)
                for (u32 x = 0; x < 400; x++)
                    framebuf[y * stride / sizeof(u32) + x] = topBuf[(x + 1) * 240 - y];
            uint32_t *botBuf = (uint32_t*)e.get_bottom_buffer();
            for (u32 y = 0; y < 240; y++)
                for (u32 x = 0; x < 320; x++)
                    framebuf[(y + 240) * stride / sizeof(u32) + (x + 40)] = botBuf[(x + 1) * 240 - y];
            framebufferEnd(&fb);
        }
        catch (EmuException::FatalError& error)
        {
            e.print_state();
            printf("Fatal emulation error occurred!\n%s\n", error.what());
            return 1;
        }
        catch (EmuException::RebootException& r)
        {
            e.reset(false);
        }
    }

    framebufferClose(&fb);
    clkrstSetClockRate(&cpuSession, 1020000000);
    clkrstExit();
    appletUnlockExit();
    return 0;
}
