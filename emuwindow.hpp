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
        QImage top_image, bottom_image;
    public:
        EmuWindow();

        bool is_running();

        void closeEvent(QCloseEvent* event) override;

        void draw(uint8_t* top_screen, uint8_t* bottom_screen);
        void paintEvent(QPaintEvent* event) override;
};

inline bool EmuWindow::is_running()
{
    return running;
}

#endif // EMUWINDOW_HPP
