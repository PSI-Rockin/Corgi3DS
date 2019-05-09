#include "rotr.hpp"

uint32_t rotl32(uint32_t n, unsigned int c)
{
  const unsigned int mask = 0x1F;

  c &= mask;
  return (n<<c) | (n>>( (-c)&mask ));
}

uint32_t rotr32(uint32_t n, unsigned int c)
{
    const unsigned int mask = 0x1F;
    c &= mask;

    uint32_t result = (n>>c) | (n<<( (-c)&mask ));

    return result;
}
