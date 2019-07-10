#ifndef P9_HLE_HPP
#define P9_HLE_HPP
#include <cstdint>
#include <string>

namespace P9_HLE
{
    std::string get_function_name(uint32_t* buffer, int len);
}

#endif // P9_HLE_HPP
