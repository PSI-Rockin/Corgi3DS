#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
#include <functional>
#include <list>
#include <vector>

//List of different frequencies
constexpr static uint64_t ARM11_CLOCKRATE = 268111856; //268 MHz
constexpr static uint64_t ARM9_CLOCKRATE = ARM11_CLOCKRATE / 2; //134 MHz
constexpr static uint64_t XTENSA_CLOCKRATE = 40 * 1000 * 1000; //~40 MHz

struct CycleCount
{
    int64_t count;
    int64_t remainder;
    uint64_t clockrate;
};

struct SchedulerEvent
{
    int64_t time_registered;
    int64_t time_to_run;
    uint64_t param;
    std::function<void(uint64_t)> func;
};

struct SchedulerCPU
{
    int64_t* cur_timestamp;
    uint64_t clockrate;
    bool active;

    std::function<void(int64_t)> run_func;
};

class Scheduler
{
    private:
        std::list<SchedulerEvent> events;

        std::vector<SchedulerCPU> cpus;
        std::list<SchedulerCPU*> active_cpus;

        int64_t closest_event_time;
        int64_t next_cpu_time;

        int next_cpu_id;

        CycleCount quantum;
        CycleCount cycles11;
        CycleCount cycles9;
        CycleCount xtensa_cycles;

        int64_t quantum_cycles;
        int64_t cycles11_to_run;
        int64_t cycles9_to_run;
        int64_t xtensa_cycles_to_run;
    public:
        constexpr static uint64_t ARM11_CLOCKRATE = 268111856;
        constexpr static uint64_t ARM9_CLOCKRATE = ARM11_CLOCKRATE / 2;

        Scheduler();

        int register_cpu(std::function<void(int64_t)> func,
                         int64_t* cur_timestamp, uint64_t clockrate);
        void stop_cpu(int id);
        void activate_cpu(int id);
        void run_cpus();

        void calculate_cycles_to_run();
        int64_t get_cycles11_to_run();
        int64_t get_cycles9_to_run();
        int64_t get_xtensa_cycles_to_run();
        void reset();

        void set_quantum_rate(uint64_t clock);
        void set_clockrate_11(uint64_t clock);
        void set_clockrate_9(uint64_t clock);
        void set_clockrate_xtensa(uint64_t clock);

        void add_event(std::function<void(uint64_t)> func, int64_t cycles, uint64_t clockrate, uint64_t param = 0);
        void process_events();
};

inline int64_t Scheduler::get_cycles11_to_run()
{
    return cycles11_to_run;
}

inline int64_t Scheduler::get_cycles9_to_run()
{
    return cycles9_to_run;
}

inline int64_t Scheduler::get_xtensa_cycles_to_run()
{
    return xtensa_cycles_to_run;
}

inline void Scheduler::set_quantum_rate(uint64_t clock)
{
    quantum.clockrate = clock;
}

inline void Scheduler::set_clockrate_9(uint64_t clock)
{
    cycles9.clockrate = clock;
}

inline void Scheduler::set_clockrate_11(uint64_t clock)
{
    cycles11.clockrate = clock;
}

inline void Scheduler::set_clockrate_xtensa(uint64_t clock)
{
    xtensa_cycles.clockrate = clock;
}

#endif // SCHEDULER_HPP
