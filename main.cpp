#include <fstream>
#include <QApplication>
#include "emulator.hpp"
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        printf("Args: [boot9] [boot11] [OTP] [NAND]\n");
        return 1;
    }

    uint8_t boot9_rom[1024 * 64], boot11_rom[1024 * 64], otp_rom[256];

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
    printf("All files loaded successfully!\n");
    e.load_roms(boot9_rom, boot11_rom, otp_rom);
    e.reset();
    while (emuwindow->is_running())
    {
        a.processEvents();
        e.run();
        emuwindow->draw(e.get_buffer());
    }

    return 0;
}
