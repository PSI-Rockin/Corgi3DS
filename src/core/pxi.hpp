#ifndef PXI_HPP
#define PXI_HPP
#include <cstdint>
#include <queue>

struct PXI_SYNC
{
    uint8_t recv_data;
    bool local_irq;
};

struct PXI_CNT
{
    bool send_empty_irq;
    bool recv_not_empty_irq;
    bool error;
    bool enable;
};

class MPCore_PMR;
class Interrupt9;

class PXI
{
    private:
        MPCore_PMR* mpcore;
        Interrupt9* int9;
        PXI_SYNC sync9, sync11;

        PXI_CNT cnt9, cnt11;

        std::queue<uint32_t> recv9, recv11;

        uint32_t last_recv9, last_recv11;
    public:
        PXI(MPCore_PMR* mpcore, Interrupt9* int9);

        void reset();

        uint32_t read_sync9();
        uint32_t read_sync11();
        uint16_t read_cnt9();
        uint16_t read_cnt11();
        void write_sync9(uint32_t value);
        void write_sync11(uint32_t value);
        void write_cnt9(uint16_t value);
        void write_cnt11(uint16_t value);

        uint32_t read_msg9();
        uint32_t read_msg11();

        void send_to_9(uint32_t value);
        void send_to_11(uint32_t value);
};

#endif // PXI_HPP
