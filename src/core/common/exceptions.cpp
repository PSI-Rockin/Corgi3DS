#include <cstdarg>
#include "exceptions.hpp"

namespace EmuException
{

ARMDataAbort::ARMDataAbort(uint32_t vaddr, bool is_write) : std::runtime_error("ARM data abort"), vaddr(vaddr),
    is_write(is_write)
{}

ARMPrefetchAbort::ARMPrefetchAbort(uint32_t vaddr) : std::runtime_error("ARM prefetch abort"), vaddr(vaddr)
{}

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
