#include <fstream>
#include <QApplication>
#include "../core/emulator.hpp"
#include "../core/common/exceptions.hpp"
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        printf("Args: [boot9] [boot11] [NAND] [SD]\n");
        return 1;
    }

    /*uint8_t boot9_rom[1024 * 64], boot11_rom[1024 * 64];

    ifstream boot9(argv[1]);
    if (!boot9.is_open())
    {
        printf("Failed to open %s\n", argv[1]);
        return 1;
    }

    boot9.read((char*)&boot9_rom, sizeof(boot9_rom));

    boot9.close();

    ifstream boot11(argv[2]);
    if (!boot11.is_open())
    {
        printf("Failed to open %s\n", argv[2]);
        return 1;
    }

    boot11.read((char*)&boot11_rom, sizeof(boot11_rom));

    boot11.close();

    Emulator e;
    if (!e.mount_nand(argv[3]))
    {
        printf("Failed to open %s\n", argv[3]);
        return 1;
    }

    if (!e.mount_sd(argv[4]))
    {
        printf("Failed to open %s\n", argv[4]);
        return 1;
    }

    if (argc > 5)
    {
        if (!e.mount_cartridge(argv[5]))
        {
            printf("Failed to open %s\n", argv[5]);
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
    e.reset();*/
    QApplication a(argc, argv);
    EmuWindow* emuwindow = new EmuWindow();
    a.exec();
    /*while (emuwindow->is_running())
    {
        a.processEvents();
        uint16_t pad = emuwindow->get_pad_state();
        e.set_pad(pad);

        if (emuwindow->is_touchscreen_pressed())
            e.set_touchscreen(emuwindow->get_touchscreen_x(), emuwindow->get_touchscreen_y());
        else
            e.clear_touchscreen();

        try
        {
            e.run();
            emuwindow->draw(e.get_top_buffer(), e.get_bottom_buffer());
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
    }*/

    return 0;
}
