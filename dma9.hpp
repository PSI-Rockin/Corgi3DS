#ifndef DMA9_HPP
#define DMA9_HPP
#include <cstdint>

struct XDMA_Chan_CTRL
{
    uint8_t endian_swap_size;
    bool inc_dest;
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
};

class Emulator;

class DMA9
{
    private:
        Emulator* e;
        //XDMA channel 8 is the DMAC manager (unknown how this is different from other channels)
        XDMA_Chan xdma_chan[8];
        uint32_t xdma_ie, xdma_if;

        uint32_t xdma_debug_instrs[2];
        bool xdma_command_set;
        uint8_t xdma_command;
        uint8_t xdma_params[0x10];
        uint8_t xdma_param_count;
        uint8_t xdma_params_needed;

        void instr_wfp(int chan);
        void instr_dmago();
        void instr_flushp();
        void instr_mov(int chan);

        void xdma_write_chan_ctrl(int chan, uint32_t value);

        void xdma_exec_debug();
        void xdma_exec_instr(uint8_t byte, int chan);
    public:
        DMA9(Emulator* e);

        void reset();
        void run_xdma();

        uint32_t read32_ndma(uint32_t addr);
        uint32_t read32_xdma(uint32_t addr);
        void write32_ndma(uint32_t addr, uint32_t value);
        void write32_xdma(uint32_t addr, uint32_t value);
};

#endif // DMA9_HPP
