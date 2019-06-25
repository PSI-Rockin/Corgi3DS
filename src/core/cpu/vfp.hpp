#ifndef VFP_HPP
#define VFP_HPP
#include <cstdint>

class VFP
{
    private:
        //32 single-precision registers or 16 double-precision registers
        uint8_t fpr[32 * sizeof(float)];
    public:
        VFP();

        float get_float(int index);
        double get_double(int index);

        void set_float(int index, float value);
        void set_double(int index, double value);
};

inline float VFP::get_float(int index)
{
    return *(float*)&fpr[index << 2];
}

inline double VFP::get_double(int index)
{
    return *(double*)&fpr[index << 3];
}

inline void VFP::set_float(int index, float value)
{
    *(float*)&fpr[index << 2] = value;
}

inline void VFP::set_double(int index, double value)
{
    *(double*)&fpr[index << 3] = value;
}

#endif // VFP_HPP
