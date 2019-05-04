#ifndef EMUWINDOW_HPP
#define EMUWINDOW_HPP
#include <QMainWindow>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QImage>

class EmuWindow : public QMainWindow
{
    Q_OBJECT
    private:
        bool running;
        QImage image;
    public:
        EmuWindow();

        bool is_running();

        void closeEvent(QCloseEvent* event) override;

        void draw(uint8_t* buffer);
        void paintEvent(QPaintEvent* event) override;
};

inline bool EmuWindow::is_running()
{
    return running;
}

#endif // EMUWINDOW_HPP
