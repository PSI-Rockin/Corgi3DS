#ifndef SETTINGS_HPP
#define SETTINGS_HPP
#include <QString>

namespace Settings
{

//Boot settings - loaded once upon starting an emulator
extern QString boot9_path;
extern QString boot11_path;
extern QString nand_path;
extern QString sd_path;

void load();
void save();

}

#endif // SETTINGS_HPP
