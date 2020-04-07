#include <algorithm>
#include "scheduler.hpp"
#include "common/exceptions.hpp"

Scheduler::Scheduler()
{

}

void Scheduler::reset()
{
    cpus.clear();
    active_cpus.clear();
    events.clear();

    //Reserve an arbitrary number of CPUs. This way, the vector will not move its contents to new memory
    //and thus make active_cpus point to invalid data when push_back is called on the vector.
    cpus.reserve(100);

    next_cpu_id = 0;
    next_cpu_time = 0;

    closest_event_time = 0;

    cycles11.count = 0;
    cycles11.remainder = 0;

    cycles9.count = 0;
    cycles9.remainder = 0;

    xtensa_cycles.count = 0;
    xtensa_cycles.remainder = 0;
}

void Scheduler::calculate_cycles_to_run()
{
    const static int MAX_CYCLES = 64;

    cycles11_to_run = 0;

    if (cycles11.count + MAX_CYCLES <= closest_event_time)
        cycles11_to_run = MAX_CYCLES;
    else
    {
        int64_t delta = closest_event_time - cycles11.count;
        if (delta > 0)
            cycles11_to_run = delta;
        else
            cycles11_to_run = 0;
    }

    //ARM9 cycles are at half speed
    cycles9_to_run = cycles11_to_run >> 1;
    cycles9.remainder += cycles11_to_run & 0x1;
    if (cycles9.remainder == 2)
    {
        cycles9_to_run++;
        cycles9.remainder = 0;
    }

    //The Xtensa runs at 40 MHz on the 3DS. This is approximately 7 times slower than the ARM11
    xtensa_cycles_to_run = cycles11_to_run / 7;
    xtensa_cycles.remainder += cycles11_to_run % 7;
    if (xtensa_cycles.remainder >= 7)
    {
        xtensa_cycles_to_run++;
        xtensa_cycles.remainder -= 7;
    }
}

int Scheduler::register_cpu(std::function<void (int64_t)> func, int64_t *cur_timestamp,
                             uint64_t clockrate)
{
    SchedulerCPU cpu;
    cpu.run_func = func;
    cpu.cur_timestamp = cur_timestamp;
    cpu.clockrate = clockrate;
    cpu.active = false;

    cpus.push_back(cpu);
    int id = next_cpu_id;
    next_cpu_id++;
    return id;
}

void Scheduler::stop_cpu(int id)
{
    if (id < 0 || id >= cpus.size())
        EmuException::die("[Scheduler] Invalid CPU id %d given to stop_cpu!", id);
    if (!cpus[id].active)
        return;

    //Just set the status bit. The run loop will remove this CPU from the list
    cpus[id].active = false;
}

void Scheduler::activate_cpu(int id)
{
    if (id < 0 || id >= cpus.size())
        EmuException::die("[Scheduler] Invalid CPU id %d given to activate_cpu!", id);
    if (cpus[id].active)
        return;
    active_cpus.push_back(&cpus[id]);
    cpus[id].active = true;
    *cpus[id].cur_timestamp = next_cpu_time;
}

void Scheduler::run_cpus()
{
    const static int64_t MAX_CYCLES = 64;
    next_cpu_time = std::min(closest_event_time, next_cpu_time + MAX_CYCLES);
    for (auto it = active_cpus.begin(); it != active_cpus.end(); )
    {
        SchedulerCPU* cpu = *(it);
        if (cpu->active)
        {
            int64_t cycles = (next_cpu_time - *cpu->cur_timestamp) << 8;
            cycles *= cpu->clockrate;
            cycles /= ARM11_CLOCKRATE;
            cpu->run_func(cycles >> 8);
            it++;
        }
        else
        {
            //If the CPU had been halted previously, remove it from the active list
            it = active_cpus.erase(it);
        }
    }
}

void Scheduler::add_event(std::function<void(uint64_t)> func, int64_t cycles, uint64_t param)
{
    SchedulerEvent event;

    event.func = func;
    event.time_registered = -1; //TODO
    event.time_to_run = cycles11.count + cycles;
    event.param = param;

    closest_event_time = std::min(event.time_to_run, closest_event_time);

    events.push_back(event);
}

void Scheduler::process_events()
{
    cycles11.count += cycles11_to_run;
    cycles9.count += cycles9_to_run;
    xtensa_cycles.count += xtensa_cycles_to_run;
    if (next_cpu_time >= closest_event_time)
    {
        int64_t new_time = 0x7FFFFFFFULL << 32ULL;
        for (auto it = events.begin(); it != events.end(); )
        {
            if (it->time_to_run <= closest_event_time)
            {
                it->func(it->param);
                it = events.erase(it);
            }
            else
            {
                new_time = std::min(it->time_to_run, new_time);
                it++;
            }
        }
        closest_event_time = new_time;
    }
}
