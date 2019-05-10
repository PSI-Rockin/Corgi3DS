#include <fstream>
#include <QApplication>
#include "../core/emulator.hpp"
#include "../core/common/exceptions.hpp"
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    if (argc < 7)
    {
        printf("Args: [boot9] [boot11] [OTP] [NAND] [NAND CID] [SD]\n");
        return 1;
    }

    uint8_t boot9_rom[1024 * 64], boot11_rom[1024 * 64], otp_rom[256], cid_rom[16];

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

    ifstream otp(argv[3]);
    if (!otp.is_open())
    {
        printf("Failed to open %s\n", argv[3]);
        return 1;
    }

    otp.read((char*)&otp_rom, sizeof(otp_rom));

    otp.close();

    QApplication a(argc, argv);
    EmuWindow* emuwindow = new EmuWindow();

    Emulator e;
    if (!e.mount_nand(argv[4]))
    {
        printf("Failed to open %s\n", argv[4]);
        return 1;
    }

    ifstream cid(argv[5]);
    if (!cid.is_open())
    {
        printf("Failed to open %s\n", argv[5]);
        return 1;
    }

    if (!e.mount_sd(argv[6]))
    {
        printf("Failed to open %s\n", argv[6]);
        return 1;
    }

    cid.read((char*)&cid_rom, sizeof(cid_rom));
    cid.close();

    printf("All files loaded successfully!\n");
    e.load_roms(boot9_rom, boot11_rom, otp_rom, cid_rom);
    e.reset();
    while (emuwindow->is_running())
    {
        a.processEvents();
        uint16_t pad = emuwindow->get_pad_state();
        e.set_pad(pad);
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
    }

    return 0;
}
