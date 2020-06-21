#ifndef EMUTHREAD_HPP
#define EMUTHREAD_HPP
#include <QString>
#include <QThread>

#include "../core/emulator.hpp"

class EmuThread : public QThread
{
    Q_OBJECT
    private:
        Emulator e;
    public:
        EmuThread();

        bool boot_emulator(QString cart_path);
    protected:
        void run() override;
    signals:
        void boot_error(QString message);
};

#endif // EMUTHREAD_HPP
