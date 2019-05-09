#ifndef ROTR_HPP
#define ROTR_HPP
#include <cstdint>

//Thanks to Stack Overflow for providing a safe rotate function
uint32_t rotl32(uint32_t n, unsigned int c);
uint32_t rotr32(uint32_t n, unsigned int c);

#endif // ROTR_HPP
