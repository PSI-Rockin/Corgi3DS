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
