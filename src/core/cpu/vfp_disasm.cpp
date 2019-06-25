#include <sstream>
#include "arm.hpp"
#include "arm_disasm.hpp"

using namespace std;

namespace ARM_Disasm
{

string vfp_load_store(uint32_t instr)
{
    bool p = (instr >> 24) & 0x1;
    bool u = (instr >> 23) & 0x1;
    bool w = (instr >> 21) & 0x1;
    bool l = (instr >> 20) & 0x1;

    uint16_t op = (p << 3) | (u << 2) | (w << 1) | l;

    switch (op)
    {
        case 0x06:
        case 0x07:
            return vfp_load_store_block(instr);
        default:
        {
            stringstream und_output;
            und_output << "undefined vfp load/store 0x" << hex << op;
            return und_output.str();
        }
    }
}

string vfp_load_store_block(uint32_t instr)
{
    stringstream output;

    bool p = (instr >> 24) & 0x1;
    bool u = (instr >> 23) & 0x1;
    bool w = (instr >> 21) & 0x1;
    bool l = (instr >> 20) & 0x1;
    uint32_t base = (instr >> 16) & 0xF;
    uint32_t start = (instr >> 12) & 0xF;
    bool is_double = (instr >> 8) & 0x1;
    uint16_t offset = instr & 0xFF;
    if (is_double)
        offset >>= 1;

    if (l)
        output << "fldm";
    else
        output << "fstm";

    if (u)
        output << "ia";
    else
        output << "db";

    string precision;

    if (is_double)
        precision = "d";
    else
        precision = "s";

    output << precision;

    output << cond_name(instr >> 28) << " ";

    output << ARM_CPU::get_reg_name(base);
    if (w)
        output << "!";

    output << ", {";

    if ((instr >> 22) & 0x1)
        start += 16;

    uint32_t end = start + offset - 1;
    output << precision << dec << start << "-" << precision << end << "}";

    return output.str();
}

};
