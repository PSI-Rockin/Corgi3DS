#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QMatrix>
#include "emuwindow.hpp"

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
    pad_state = 0;
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

void EmuWindow::press_key(HID_PAD_STATE state)
{
    pad_state |= 1 << state;
}

void EmuWindow::release_key(HID_PAD_STATE state)
{
    pad_state &= ~(1 << state);
}
