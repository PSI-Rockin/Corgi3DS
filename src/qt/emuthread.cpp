#include <fstream>

#include "emuthread.hpp"
#include "settings.hpp"

#include "../core/common/exceptions.hpp"

using namespace std;

EmuThread::EmuThread()
{
    quit = true;
    has_frame_settings = false;
}

void EmuThread::run()
{
    while (!quit)
    {
        while (!has_frame_settings);

        has_frame_settings = false;
        try
        {
            e.run();
            emit frame_complete(e.get_top_buffer(), e.get_bottom_buffer());
        }
        catch (EmuException::FatalError& error)
        {
            e.print_state();
            emit emu_error(error.what());
            quit = true;
            break;
        }
        catch (EmuException::RebootException& r)
        {
            e.reset(false);
        }
    }
}

//Returns true if all settings are parsed correctly, allowing the UI thread to start the emuthread.
bool EmuThread::boot_emulator(QString cart_path)
{
    //This function runs in the UI thread. Hence, it is safe for it to access Settings.
    quit = true;

    uint8_t boot9_rom[1024 * 64], boot11_rom[1024 * 64];

    ifstream boot9(Settings::boot9_path.toStdString());
    if (!boot9.is_open())
    {
        emit boot_error(tr("Failed to open ARM9 boot ROM."));
        return false;
    }

    boot9.read((char*)&boot9_rom, sizeof(boot9_rom));

    boot9.close();

    ifstream boot11(Settings::boot11_path.toStdString());
    if (!boot11.is_open())
    {
        emit boot_error(tr("Failed to open ARM11 boot ROM."));
        return false;
    }

    boot11.read((char*)&boot11_rom, sizeof(boot11_rom));

    boot11.close();

    e.load_roms(boot9_rom, boot11_rom);

    if (!e.mount_nand(Settings::nand_path.toStdString()))
    {
        emit boot_error("Failed to load NAND image.");
        return false;
    }

    if (!Settings::sd_path.isEmpty())
    {
        if (!e.mount_sd(Settings::sd_path.toStdString()))
        {
            emit boot_error("Failed to load SD image.");
            return false;
        }
    }

    if (!cart_path.isEmpty())
    {
        if (!e.mount_cartridge(cart_path.toStdString()))
        {
            emit boot_error("Failed to load 3DS cartridge.");
            return false;
        }
    }

    if (!e.parse_essentials())
    {
        emit boot_error("Failed to find OTP and CID. Please make sure your NAND is dumped from the "
                        "latest version of GodMode9.");
        return false;
    }

    e.reset();

    quit = false;
    has_frame_settings = false;

    return true;
}

void EmuThread::pass_frame_settings(FrameSettings *f)
{
    e.set_pad(f->pad_state);

    if (f->touchscreen_pressed)
        e.set_touchscreen(f->touchscreen_x, f->touchscreen_y);
    else
        e.clear_touchscreen();

    if (f->power_button)
        e.power_button();

    if (f->home_button ^ f->old_home_button)
    {
        f->old_home_button = f->home_button;
        e.home_button(f->home_button);
    }

    has_frame_settings = true;
}
