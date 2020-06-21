#ifndef EMUTHREAD_HPP
#define EMUTHREAD_HPP
#include <QString>
#include <QThread>

#include "../core/emulator.hpp"

class EmuThread : public QThread
{
    Q_OBJECT
    private:
        bool quit;
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
};

#endif // EMUTHREAD_HPP
