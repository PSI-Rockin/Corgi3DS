#ifndef VFP_HPP
#define VFP_HPP
#include <cstdint>
#include <cstdio>

struct VFP_FPSCR
{
    uint8_t vector_len;
    uint8_t vector_stride;
    uint8_t round_mode;
    bool overflow;
    bool carry;
    bool zero;
    bool negative;
};

class VFP
{
    private:
        //32 single-precision registers or 16 double-precision registers
        uint8_t fpr[32 * sizeof(float)];

        uint32_t fpexc;
        VFP_FPSCR fpscr;
    public:
        VFP();

        uint32_t get_fpexc();
        uint32_t get_fpscr();
        void set_fpexc(uint32_t value);
        void set_fpscr(uint32_t value);

        float get_float(int index);
        double get_double(int index);

        uint32_t get_reg32(int index);
        uint64_t get_reg64(int index);

        void set_float(int index, float value);
        void set_double(int index, double value);

        void set_reg32(int index, uint32_t value);
        void set_reg64(int index, uint64_t value);

        template <typename T> void cmp(T a, T b);
};

inline float VFP::get_float(int index)
{
    //printf("[VFP] Get float reg%d: %f\n", index, *(float*)&fpr[index << 2]);
    return *(float*)&fpr[index << 2];
}

inline double VFP::get_double(int index)
{
    //printf("[VFP] Get double reg%d: %f\n", index, *(double*)&fpr[index << 3]);
    return *(double*)&fpr[index << 3];
}

inline uint32_t VFP::get_reg32(int index)
{
    //printf("[VFP] Get32 reg%d: $%08X\n", index, *(uint32_t*)&fpr[index << 2]);
    return *(uint32_t*)&fpr[index << 2];
}

inline uint64_t VFP::get_reg64(int index)
{
    //printf("[VFP] Get64 reg%d: $%llX\n", index, *(uint64_t*)&fpr[index << 3]);
    return *(uint64_t*)&fpr[index << 3];
}

inline void VFP::set_float(int index, float value)
{
    //printf("[VFP] Set float reg%d: %f\n", index, value);
    *(float*)&fpr[index << 2] = value;
}

inline void VFP::set_double(int index, double value)
{
    //printf("[VFP] Set double reg%d: %f\n", index, value);
    *(double*)&fpr[index << 3] = value;
}

inline void VFP::set_reg32(int index, uint32_t value)
{
    //printf("[VFP] Set32 reg%d: $%08X\n", index, value);
    *(uint32_t*)&fpr[index << 2] = value;
}

inline void VFP::set_reg64(int index, uint64_t value)
{
    //printf("[VFP] Set64 reg%d: $%llX\n", index, value);
    *(uint64_t*)&fpr[index << 3] = value;
}

template <typename T>
inline void VFP::cmp(T a, T b)
{
    fpscr.negative = a < b;
    fpscr.zero = a == b;
    fpscr.carry = !fpscr.negative;

    //TODO: This flag is supposed to be set if either a or b is a NaN
    fpscr.overflow = false;
}

#endif // VFP_HPP
