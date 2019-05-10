#include <cstdarg>
#include "exceptions.hpp"

namespace EmuException
{

void die(const char* format, ...)
{
    //Display a message box and forcibly terminate emulation
    char output[ERROR_STRING_MAX_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(output, ERROR_STRING_MAX_LENGTH, format, args);
    va_end(args);
    throw FatalError(output);
}

void reboot()
{
    printf("Reboot signal sent to MCU!\n");
    throw RebootException("Reboot");
}

};
