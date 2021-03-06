#ifndef DMA9_HPP
#define DMA9_HPP
#include <cstdint>
#include <queue>
#include "../corelink_dma.hpp"

enum NDMA_Request
{
    NDMA_TIMER0,
    NDMA_TIMER1,
    NDMA_TIMER2,
    NDMA_TIMER3,
    NDMA_CTRCARD0,
    NDMA_CTRCARD1,
    NDMA_MMC1,
    NDMA_MMC2,
    NDMA_AES_WRITEFREE,
    NDMA_AES_READFREE,
    NDMA_SHA_IN,
    NDMA_SHA_OUT,
    NDMA_AES2 = 15
};

struct NDMA_Chan
{
    uint32_t source_addr, dest_addr;

    //Internal registers that source and dest are copied to; incremented during operation
    uint32_t int_src, int_dest;
    uint32_t transfer_count;
    uint32_t write_count;
    uint32_t fill_data;

    uint8_t dest_update_method, src_update_method;
    bool dest_reload, src_reload;
    uint8_t words_per_block;

    NDMA_Request startup_mode;
    bool imm_mode;
    bool repeating_mode;
    bool irq_enable;
    bool busy;
};

class Emulator;
class Interrupt9;
class Scheduler;

class DMA9
{
    private:
        Emulator* e;
        Interrupt9* int9;
        Scheduler* scheduler;

        uint32_t global_ndma_ctrl;

        bool pending_ndma_reqs[16];

        NDMA_Chan ndma_chan[8];
        Corelink_DMA xdma;

        void run_ndma(int chan);
    public:
        DMA9(Emulator* e, Interrupt9* int9, Scheduler* scheduler);

        void reset();
        void process_ndma_reqs();
        void run_xdma();

        void try_ndma_transfer(NDMA_Request req);
        void try_ndma_transfer_event(NDMA_Request req);

        void set_ndma_req(NDMA_Request req);
        void clear_ndma_req(NDMA_Request req);
        void set_xdma_req(XDMA_Request req);
        void clear_xdma_req(XDMA_Request req);

        uint32_t read32_ndma(uint32_t addr);
        uint32_t read32_xdma(uint32_t addr);
        void write32_ndma(uint32_t addr, uint32_t value);
        void write32_xdma(uint32_t addr, uint32_t value);
};

#endif // DMA9_HPP
