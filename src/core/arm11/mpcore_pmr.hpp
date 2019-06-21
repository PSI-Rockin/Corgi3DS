#ifndef MPCORE_PMR_HPP
#define MPCORE_PMR_HPP
#include <cstdint>

class ARM_CPU;
class Timers;

class MPCore_PMR
{
    private:
        ARM_CPU *appcore, *syscore;
        Timers* timers;
        uint32_t irq_cause[4];

        uint32_t private_int_pending[4];
        uint32_t global_int_mask[8], global_int_pending[8];
        uint32_t int_targets[7];

        void set_int_signal(ARM_CPU* core, int id_of_requester = 0);
    public:
        MPCore_PMR(ARM_CPU* appcore, ARM_CPU* syscore, Timers* timers);

        void reset();
        void assert_hw_irq(int id);
        void assert_private_irq(int id, int core);

        uint8_t read8(int core, uint32_t addr);
        uint32_t read32(int core, uint32_t addr);
        void write8(int core, uint32_t addr, uint8_t value);
        void write16(int core, uint32_t addr, uint16_t value);
        void write32(int core, uint32_t addr, uint32_t value);
};

#endif // MPCORE_PMR_HPP
