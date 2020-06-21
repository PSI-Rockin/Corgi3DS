#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "settings.hpp"
#include "settingswindow.hpp"

SettingsWindow::SettingsWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(tr("Settings"));

    Settings::load();

    boot9_info = new QLabel(tr("No ROM selected"));
    boot11_info = new QLabel(tr("No ROM selected"));
    nand_info = new QLabel(tr("No image selected"));
    sd_info = new QLabel(tr("No image selected"));

    settings_update();

    QPushButton* boot9_button = new QPushButton(tr("Browse..."));
    QPushButton* boot11_button = new QPushButton(tr("Browse..."));
    QPushButton* nand_button = new QPushButton(tr("Browse..."));
    QPushButton* sd_button = new QPushButton(tr("Browse..."));

    connect(boot9_button, &QPushButton::pressed, this, [=]() {
        QString file_name = QFileDialog::getOpenFileName(this, tr("Open ARM9 boot ROM"), "", "Binary file (*.bin)");
        Settings::boot9_path = file_name;
        printf("Boot9: %s\n", Settings::boot9_path.toStdString().c_str());
        settings_update();
    });

    connect(boot11_button, &QPushButton::pressed, this, [=]() {
        QString file_name = QFileDialog::getOpenFileName(this, tr("Open ARM11 boot ROM"), "", "Binary file (*.bin)");
        Settings::boot11_path = file_name;
        settings_update();
    });

    connect(nand_button, &QPushButton::pressed, this, [=] {
        QString file_name = QFileDialog::getOpenFileName(this, tr("Open NAND Image"), "", "Binary file (*.bin)");
        Settings::nand_path = file_name;
        settings_update();
    });

    connect(sd_button, &QPushButton::pressed, this, [=] {
        QString file_name = QFileDialog::getOpenFileName(this, tr("Open SD Image"), "", "Binary file (*.bin)");
        Settings::sd_path = file_name;
        settings_update();
    });

    QGridLayout* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("ARM9 boot ROM")), 0, 0);
    grid->addWidget(boot9_info, 0, 1);
    grid->addWidget(boot9_button, 0, 2);

    grid->addWidget(new QLabel(tr("ARM11 boot ROM")), 1, 0);
    grid->addWidget(boot11_info, 1, 1);
    grid->addWidget(boot11_button, 1, 2);

    grid->addWidget(new QLabel(tr("NAND")), 2, 0);
    grid->addWidget(nand_info, 2, 1);
    grid->addWidget(nand_button, 2, 2);

    grid->addWidget(new QLabel(tr("SD")), 3, 0);
    grid->addWidget(sd_info, 3, 1);
    grid->addWidget(sd_button, 3, 2);

    QGroupBox* system_files_box = new QGroupBox(tr("System Files"));
    system_files_box->setLayout(grid);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(system_files_box);

    setLayout(layout);

    setMinimumWidth(400);
}

void SettingsWindow::settings_update()
{
    if (!Settings::boot9_path.isEmpty())
        boot9_info->setText(QFileInfo(Settings::boot9_path).fileName());
    else
        boot9_info->setText(tr("No ROM selected"));

    if (!Settings::boot11_path.isEmpty())
        boot11_info->setText(QFileInfo(Settings::boot11_path).fileName());
    else
        boot11_info->setText(tr("No ROM selected"));

    if (!Settings::nand_path.isEmpty())
        nand_info->setText(QFileInfo(Settings::nand_path).fileName());
    else
        nand_info->setText(tr("No image selected"));

    if (!Settings::sd_path.isEmpty())
        sd_info->setText(QFileInfo(Settings::sd_path).fileName());
    else
        sd_info->setText(tr("No image selected"));

    Settings::save();
}
