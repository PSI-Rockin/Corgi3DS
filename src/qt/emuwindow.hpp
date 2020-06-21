#ifndef EMUWINDOW_HPP
#define EMUWINDOW_HPP
#include <QMainWindow>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>

#include <cstdint>

#include "settingswindow.hpp"

enum HID_PAD_STATE
{
    PAD_A,
    PAD_B,
    PAD_SELECT,
    PAD_START,
    PAD_RIGHT,
    PAD_LEFT,
    PAD_UP,
    PAD_DOWN,
    PAD_R,
    PAD_L,
    PAD_X,
    PAD_Y
};

class EmuWindow : public QMainWindow
{
    Q_OBJECT
    private:
        SettingsWindow* settings_window;
        bool running;
        QImage top_image, bottom_image;

        uint16_t pad_state;

        bool touchscreen_pressed;
        uint16_t touchscreen_x, touchscreen_y;

        void press_key(HID_PAD_STATE state);
        void release_key(HID_PAD_STATE state);
    public:
        EmuWindow();

        uint16_t get_pad_state();
        bool is_running();

        bool is_touchscreen_pressed();
        uint16_t get_touchscreen_x();
        uint16_t get_touchscreen_y();

        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void closeEvent(QCloseEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        void draw(uint8_t* top_screen, uint8_t* bottom_screen);
        void paintEvent(QPaintEvent* event) override;
};

inline bool EmuWindow::is_running()
{
    return running;
}

inline uint16_t EmuWindow::get_pad_state()
{
    return pad_state;
}

inline bool EmuWindow::is_touchscreen_pressed()
{
    return touchscreen_pressed;
}

inline uint16_t EmuWindow::get_touchscreen_x()
{
    return touchscreen_x;
}

inline uint16_t EmuWindow::get_touchscreen_y()
{
    return touchscreen_y;
}

#endif // EMUWINDOW_HPP
