#ifndef DMA9_HPP
#define DMA9_HPP
#include <cstdint>
#include <queue>

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

enum XDMA_Request
{
    XDMA_CTRCARD,
    XDMA_SHA = 7
};

struct XDMA_Chan_CTRL
{
    uint8_t endian_swap_size;
    uint8_t dest_burst_len;
    uint16_t dest_burst_size;
    bool inc_dest;
    uint8_t src_burst_len;
    uint16_t src_burst_size;
    bool inc_src;
};

struct XDMA_Chan
{
    enum Status
    {
        STOP,
        EXEC,
        CACHEMISS,
        UPDATEPC,
        WFE,
        BARRIER,
        WFP = 7,
        KILL,
        COMPLETE,
        FAULTCOMPLETE = 0xE,
        FAULT
    };

    XDMA_Chan_CTRL ctrl;

    Status state;
    uint32_t PC;
    uint32_t source_addr, dest_addr;
    uint16_t loop_ctr[2];

    int peripheral;

    std::queue<uint32_t> fifo;
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
        bool pending_xdma_reqs[32];

        NDMA_Chan ndma_chan[8];

        //XDMA channel 8 is the DMAC manager (unknown how this is different from other channels)
        XDMA_Chan xdma_chan[9];
        uint32_t xdma_ie, xdma_if, xdma_sev_if;

        uint32_t xdma_debug_instrs[2];
        bool xdma_command_set;
        uint8_t xdma_command;
        uint8_t xdma_params[0x10];
        uint8_t xdma_param_count;
        uint8_t xdma_params_needed;

        void run_ndma(int chan);

        void instr_ld(int chan, uint8_t modifier);
        void instr_st(int chan, uint8_t modifier);
        void instr_lp(int chan, int loop_ctr_index);
        void instr_ldp(int chan, bool burst);
        void instr_wfp(int chan);
        void instr_sev(int chan);
        void instr_flushp();
        void instr_lpend(int chan);
        void instr_dmago();
        void instr_mov(int chan);

        void xdma_write_chan_ctrl(int chan, uint32_t value);

        void xdma_exec_debug();
        void xdma_exec_instr(uint8_t byte, int chan);
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
