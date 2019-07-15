#include <algorithm>
#include "scheduler.hpp"

Scheduler::Scheduler()
{

}

void Scheduler::reset()
{
    events.clear();

    closest_event_time = 0;
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
    if (cycles9.remainder && (cycles11_to_run & 0x1))
        cycles9_to_run++;
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
    if (cycles11.count >= closest_event_time)
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
