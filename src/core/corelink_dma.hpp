#ifndef CORELINK_DMA_HPP
#define CORELINK_DMA_HPP
#include <cstdint>
#include <functional>
#include <queue>

enum XDMA_Request
{
    XDMA_CTRCARD,
    XDMA_SHA = 7
};

struct Corelink_Chan_CTRL
{
    uint8_t endian_swap_size;
    uint8_t dest_burst_len;
    uint16_t dest_burst_size;
    bool inc_dest;
    uint8_t src_burst_len;
    uint16_t src_burst_size;
    bool inc_src;
};

struct Corelink_Chan
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

    Corelink_Chan_CTRL ctrl;

    Status state;
    uint32_t PC;
    uint32_t source_addr, dest_addr;
    uint16_t loop_ctr[2];

    int peripheral;

    std::queue<uint32_t> fifo;
};

class Corelink_DMA
{
    private:
        std::function<uint8_t(uint32_t)> mem_read8;
        std::function<uint32_t(uint32_t)> mem_read32;
        std::function<void(uint32_t, uint32_t)> mem_write32;
        std::function<void(int)> send_interrupt;

        bool pending_reqs[32];

        //XDMA channel 8 is the DMAC manager (unknown how this is different from other channels)
        Corelink_Chan dma[9];
        uint32_t int_enable, int_flag, sev_flag;

        uint32_t debug_instrs[2];
        bool command_set;
        uint8_t command;
        uint8_t params[0x10];
        uint8_t param_count;
        uint8_t params_needed;

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

        void write_chan_ctrl(int chan, uint32_t value);

        void exec_debug();
        void exec_instr(uint8_t byte, int chan);
    public:
        Corelink_DMA();

        void reset();
        void run();

        void set_pending(int index);
        void clear_pending(int index);

        void set_mem_read8_func(std::function<uint8_t(uint32_t)> func);
        void set_mem_read32_func(std::function<uint32_t(uint32_t)> func);
        void set_mem_write32_func(std::function<void(uint32_t, uint32_t)> func);
        void set_send_interrupt(std::function<void(int)> func);

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // CORELINK_DMA_HPP
