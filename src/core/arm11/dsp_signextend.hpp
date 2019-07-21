#ifndef DSP_SIGNEXTEND_HPP
#define DSP_SIGNEXTEND_HPP

//BitSize and SignExtend taken from Teakra
template <typename T>
constexpr unsigned BitSize() {
    return sizeof(T) * 8; // yeah I know I shouldn't use 8 here.
}

template <typename T>
constexpr T SignExtend(const T value, unsigned bit_count) {
    const T mask = static_cast<T>(1ULL << bit_count) - 1;
    const bool sign_bit = ((value >> (bit_count - 1)) & 1) != 0;
    if (sign_bit) {
        return value | ~mask;
    }
    return value & mask;
}

template <unsigned bit_count, typename T>
constexpr T SignExtend(const T value) {
    static_assert(bit_count <= BitSize<T>(), "bit_count larger than bitsize of T");
    return SignExtend(value, bit_count);
}

#endif // DSP_SIGNEXTEND_HPP
