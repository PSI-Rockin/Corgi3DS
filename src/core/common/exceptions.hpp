#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP
#include <stdexcept>
#include <cstdint>

#define ERROR_STRING_MAX_LENGTH 255

namespace EmuException
{
    class FatalError : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    class RebootException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    class ARMDataAbort : public std::runtime_error
    {
        using std::runtime_error::runtime_error;

        public:
            ARMDataAbort(uint32_t vaddr);
            uint32_t vaddr;
    };

    void die(const char* format, ...);
    void reboot();
};

#endif // EXCEPTIONS_HPP
