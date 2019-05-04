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
    resize(400, 240);
    show();
    running = true;
}

void EmuWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    running = false;
}

void EmuWindow::draw(uint8_t *buffer)
{
    image = QImage(buffer, 240, 400, QImage::Format_RGBA8888);
    QPoint center = image.rect().center();
    QMatrix matrix;
    matrix.translate(center.x(), center.y());
    matrix.rotate(270);
    image = image.transformed(matrix);

    update();
}

void EmuWindow::paintEvent(QPaintEvent *event)
{
    event->accept();

    QPainter painter(this);

    QRect widget_rect(
        0, 0,
        painter.device()->width(),
        painter.device()->height()
    );

    QRect src_rect(image.rect());

    src_rect.moveCenter(widget_rect.center());
    painter.drawImage(src_rect.topLeft(), image);
}
