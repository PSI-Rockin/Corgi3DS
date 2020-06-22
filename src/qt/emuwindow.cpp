#include <QAction>
#include <QFileDialog>
#include <QImage>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QMatrix>
#include <QMessageBox>
#include "emuwindow.hpp"
#include "settings.hpp"

EmuWindow::EmuWindow()
{
    QPalette palette;
    palette.setColor(QPalette::Background, Qt::black);
    setPalette(palette);
    setAutoFillBackground(true);
    setWindowTitle("Corgi3DS");
    resize(400, 240 + 240);
    show();
    running = true;
    frame_settings.touchscreen_pressed = false;
    frame_settings.pad_state = 0;

    settings_window = new SettingsWindow;

    init_menu_bar();

    connect(&emuthread, &EmuThread::boot_error, this, &EmuWindow::display_boot_error);
    connect(&emuthread, &EmuThread::frame_complete, this, &EmuWindow::frame_complete);
    connect(&emuthread, &EmuThread::emu_error, this, &EmuWindow::display_emu_error);
    connect(this, &EmuWindow::pass_frame_settings, &emuthread, &EmuThread::pass_frame_settings);
}

void EmuWindow::init_menu_bar()
{
    auto settings_action = new QAction(tr("&Settings"), this);
    connect(settings_action, &QAction::triggered, this, [=]() {
        settings_window->show();
    });

    auto options_menu = menuBar()->addMenu(tr("&Options"));
    options_menu->addAction(settings_action);

    auto open_cart_action = new QAction(tr("&Open 3DS cartridge..."), this);
    connect(open_cart_action, &QAction::triggered, this, [=]() {
        QString file_name = QFileDialog::getOpenFileName(this, tr("Open 3DS cartridge"), "", "3DS cartridge (*.3ds)");
        boot_emulator(file_name);
    });

    auto no_cart_boot_action = new QAction(tr("Boot without cartridge"), this);
    connect(no_cart_boot_action, &QAction::triggered, this, [=]() {
        boot_emulator("");
    });

    auto file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(open_cart_action);
    file_menu->addAction(no_cart_boot_action);
}

void EmuWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    running = false;
}

void EmuWindow::draw(uint8_t *top_screen, uint8_t *bottom_screen)
{
    top_image = QImage(top_screen, 240, 400, QImage::Format_RGBA8888);
    QPoint center = top_image.rect().center();
    QMatrix top_matrix, bottom_matrix;
    top_matrix.translate(center.x(), center.y());
    top_matrix.rotate(270);
    top_image = top_image.transformed(top_matrix);

    bottom_image = QImage(bottom_screen, 240, 320, QImage::Format_RGBA8888);
    center = bottom_image.rect().center();
    bottom_matrix.translate(center.x(), center.y());
    bottom_matrix.rotate(270);
    bottom_image = bottom_image.transformed(bottom_matrix);

    update();
}

void EmuWindow::paintEvent(QPaintEvent *event)
{
    event->accept();

    QPainter painter(this);

    QRect top_widget_rect(0, 0, 400, 240);
    QRect top_src_rect(top_image.rect());
    top_src_rect.moveCenter(top_widget_rect.center());
    painter.drawImage(top_src_rect.topLeft(), top_image);

    QRect bottom_widget_rect(40, 240, 320, 240);
    QRect bottom_src_rect(bottom_image.rect());
    bottom_src_rect.moveCenter(bottom_widget_rect.center());
    painter.drawImage(bottom_src_rect.topLeft(), bottom_image);
}

void EmuWindow::keyPressEvent(QKeyEvent *event)
{
    event->accept();

    switch (event->key())
    {
        case Qt::Key_Up:
            press_key(PAD_UP);
            break;
        case Qt::Key_Down:
            press_key(PAD_DOWN);
            break;
        case Qt::Key_Left:
            press_key(PAD_LEFT);
            break;
        case Qt::Key_Right:
            press_key(PAD_RIGHT);
            break;
        case Qt::Key_Z:
            press_key(PAD_B);
            break;
        case Qt::Key_X:
            press_key(PAD_A);
            break;
        case Qt::Key_A:
            press_key(PAD_Y);
            break;
        case Qt::Key_S:
            press_key(PAD_X);
            break;
        case Qt::Key_Q:
            press_key(PAD_L);
            break;
        case Qt::Key_W:
            press_key(PAD_R);
            break;
        case Qt::Key_Return:
            press_key(PAD_START);
            break;
        case Qt::Key_Space:
            press_key(PAD_SELECT);
            break;
    }
}

void EmuWindow::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();

    switch (event->key())
    {
        case Qt::Key_Up:
            release_key(PAD_UP);
            break;
        case Qt::Key_Down:
            release_key(PAD_DOWN);
            break;
        case Qt::Key_Left:
            release_key(PAD_LEFT);
            break;
        case Qt::Key_Right:
            release_key(PAD_RIGHT);
            break;
        case Qt::Key_Z:
            release_key(PAD_B);
            break;
        case Qt::Key_X:
            release_key(PAD_A);
            break;
        case Qt::Key_A:
            release_key(PAD_Y);
            break;
        case Qt::Key_S:
            release_key(PAD_X);
            break;
        case Qt::Key_Q:
            release_key(PAD_L);
            break;
        case Qt::Key_W:
            release_key(PAD_R);
            break;
        case Qt::Key_Return:
            release_key(PAD_START);
            break;
        case Qt::Key_Space:
            release_key(PAD_SELECT);
            break;
    }
}

void EmuWindow::mousePressEvent(QMouseEvent *event)
{
    event->accept();

    if (event->y() >= 240 && event->x() >= 40 && event->x() < 40 + 320)
    {
        frame_settings.touchscreen_pressed = true;
        frame_settings.touchscreen_x = event->x() - 40;
        frame_settings.touchscreen_y = event->y() - 240;
    }
    else
        frame_settings.touchscreen_pressed = false;
}

void EmuWindow::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();

    frame_settings.touchscreen_pressed = false;
}

void EmuWindow::press_key(HID_PAD_STATE state)
{
    frame_settings.pad_state |= 1 << state;
}

void EmuWindow::release_key(HID_PAD_STATE state)
{
    frame_settings.pad_state &= ~(1 << state);
}

void EmuWindow::boot_emulator(QString cart_path)
{
    if (emuthread.boot_emulator(cart_path))
    {
        emuthread.pass_frame_settings(&frame_settings);
        emuthread.start();
    }
}

void EmuWindow::frame_complete(uint8_t *top_screen, uint8_t *bottom_screen)
{
    draw(top_screen, bottom_screen);

    emuthread.pass_frame_settings(&frame_settings);
}

void EmuWindow::display_boot_error(QString message)
{
    QMessageBox msgBox;
    msgBox.setText("Error occurred during boot");
    msgBox.setInformativeText(message);
    msgBox.setStandardButtons(QMessageBox::Abort);
    msgBox.setDefaultButton(QMessageBox::Abort);
    msgBox.exec();
}

void EmuWindow::display_emu_error(QString message)
{
    QMessageBox msgBox;
    msgBox.setText("Emulation has been terminated");
    msgBox.setInformativeText(message);
    msgBox.setStandardButtons(QMessageBox::Abort);
    msgBox.setDefaultButton(QMessageBox::Abort);
    msgBox.exec();
}
