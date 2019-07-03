#ifndef MPCORE_PMR_HPP
#define MPCORE_PMR_HPP
#include <cstdint>

class ARM_CPU;
class Timers;

struct LocalIrqController
{
    uint32_t irq_cause;
    uint32_t cur_active_irq;
    uint32_t highest_priority_pending;
    uint8_t preemption_mask;
    uint8_t priority_mask;
    bool enabled;
};

class MPCore_PMR
{
    private:
        ARM_CPU *appcore, *syscore;
        Timers* timers;
        LocalIrqController local_irq_ctrl[4];

        //Only for interrupts 0-15. Contains the ID of the core that requested an SWI.
        int private_int_requestor[4][16];

        uint32_t private_int_pending[4][8];
        uint32_t private_int_active[4][8];
        uint32_t global_int_mask[8];

        uint8_t private_int_priority[4][32];
        uint8_t global_int_priority[96];
        uint8_t global_int_targets[96];

        uint8_t get_int_priority(int core, int int_id);
        uint32_t find_highest_priority_pending(int core);

        void check_if_can_assert_irq(int core);
        void set_int_signal(int core, bool irq);
    public:
        constexpr static uint32_t SPURIOUS_INT = 0x3FF;
        MPCore_PMR(ARM_CPU* appcore, ARM_CPU* syscore, Timers* timers);

        void reset();
        void assert_hw_irq(int id);
        void set_pending_irq(int core, int int_id, int id_of_requester = 0);

        uint8_t read8(int core, uint32_t addr);
        uint32_t read32(int core, uint32_t addr);
        void write8(int core, uint32_t addr, uint8_t value);
        void write16(int core, uint32_t addr, uint16_t value);
        void write32(int core, uint32_t addr, uint32_t value);
};

#endif // MPCORE_PMR_HPP
