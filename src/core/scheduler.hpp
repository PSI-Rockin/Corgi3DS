#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
#include <list>

class Emulator;

typedef void(Emulator::*event_func)(uint64_t param);

enum EVENT_ID
{
    TIMER9_INT,
    TIMER11_INT,
    NDMA_TRANSFER,
    NDMA_TRY_TRANSFER,
    XDMA_TRANSFER,
    GPU_MEMFILL,
    I2C_TRANSFER
};

struct CycleCount
{
    int64_t count;
    int64_t remainder;
};

struct SchedulerEvent
{
    EVENT_ID id;
    int64_t time_registered;
    int64_t time_to_run;
    uint64_t param;
    event_func func;
};

class Scheduler
{
    private:
        std::list<SchedulerEvent> events;

        int64_t closest_event_time;

        CycleCount cycles11;
        CycleCount cycles9;

        int cycles11_to_run;
        int cycles9_to_run;
    public:
        Scheduler();

        void calculate_cycles_to_run();
        int get_cycles11_to_run();
        int get_cycles9_to_run();
        void reset();

        void add_event(EVENT_ID id, event_func func, int64_t cycles, uint64_t param = 0);
        void process_events(Emulator* e);
};

inline int Scheduler::get_cycles11_to_run()
{
    return cycles11_to_run;
}

inline int Scheduler::get_cycles9_to_run()
{
    return cycles9_to_run;
}

#endif // SCHEDULER_HPP
