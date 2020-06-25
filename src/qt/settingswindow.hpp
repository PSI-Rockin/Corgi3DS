#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QLabel>
#include <QWidget>

class SettingsWindow : public QWidget
{
    Q_OBJECT
    private:
        QLabel* boot9_info;
        QLabel* boot11_info;
        QLabel* nand_info;
        QLabel* sd_info;

        void settings_update();
    public:
        explicit SettingsWindow(QWidget *parent = nullptr);
    signals:

    public slots:
};

#endif // SETTINGSWINDOW_H
