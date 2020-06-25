#include <QSettings>
#include "settings.hpp"

QString Settings::boot9_path;
QString Settings::boot11_path;
QString Settings::nand_path;
QString Settings::sd_path;

namespace Settings
{

void load()
{
    QSettings qset("CorgiCorp", "Corgi3DS");

    boot9_path = qset.value("system/boot9", "").toString();
    boot11_path = qset.value("system/boot11", "").toString();
    nand_path = qset.value("system/nand", "").toString();
    sd_path = qset.value("system/sd", "").toString();
}

void save()
{
    QSettings qset("CorgiCorp", "Corgi3DS");

    qset.setValue("system/boot9", boot9_path);
    qset.setValue("system/boot11", boot11_path);
    qset.setValue("system/nand", nand_path);
    qset.setValue("system/sd", sd_path);
}

}
