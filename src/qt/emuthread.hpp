#ifndef EMUTHREAD_HPP
#define EMUTHREAD_HPP
#include <QString>
#include <QThread>

#include <atomic>

#include "../core/emulator.hpp"

//The UI thread passes these settings to the emulator at the start of every frame.
struct FrameSettings
{
    uint16_t pad_state;

    bool touchscreen_pressed;
    uint16_t touchscreen_x, touchscreen_y;

    bool power_button;
    bool home_button;
    bool old_home_button;
};

class EmuThread : public QThread
{
    Q_OBJECT
    private:
        bool quit;
        std::atomic<bool> has_frame_settings;
        Emulator e;
    public:
        EmuThread();

        bool boot_emulator(QString cart_path);
    protected:
        void run() override;
    signals:
        void boot_error(QString message);
        void frame_complete(uint8_t* top_buffer, uint8_t* bottom_buffer);
        void emu_error(QString message);
    public slots:
        void pass_frame_settings(FrameSettings* f);
};

#endif // EMUTHREAD_HPP
