#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP
#include <stdexcept>

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

    void die(const char* format, ...);
    void reboot();
};

#endif // EXCEPTIONS_HPP
