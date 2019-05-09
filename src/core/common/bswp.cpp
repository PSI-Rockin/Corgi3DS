#include "bswp.hpp"

uint32_t bswp32(uint32_t value)
{
    return (value >> 24) |
            (((value >> 16) & 0xFF) << 8) |
            (((value >> 8) & 0xFF) << 16) |
            (value << 24);
}
