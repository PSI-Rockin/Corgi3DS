#include <cstring>
#include <cstdio>
#include "wifi_timers.hpp"

WiFi_Timers::WiFi_Timers()
{
    send_soc_irq = nullptr;
}

void WiFi_Timers::reset()
{
    memset(timers, 0, sizeof(timers));
}

void WiFi_Timers::run(int cycles)
{
    for (int i = 0; i < 5; i++)
    {
        if (timers[i].enabled)
        {
            timers[i].count += cycles;
            if (timers[i].count >= timers[i].target)
            {
                send_soc_irq(6 + i);
                timers[i].int_status = true;
            }
        }
    }
}

void WiFi_Timers::set_send_soc_irq(std::function<void (int)> func)
{
    send_soc_irq = func;
}

uint32_t WiFi_Timers::read_target(int index)
{
    if (index == 4)
        return timers[index].target << 12;
    return timers[index].target;
}

uint32_t WiFi_Timers::read_count(int index)
{
    if (index == 4)
        return timers[index].count << 12;
    return timers[index].count;
}

uint32_t WiFi_Timers::read_ctrl(int index)
{
    uint32_t reg = 0;
    reg |= timers[index].auto_restart << 1;
    reg |= timers[index].enabled << 2;
    return reg;
}

uint32_t WiFi_Timers::read_int_status(int index)
{
    return timers[index].int_status;
}

void WiFi_Timers::write_int_status(int index, uint32_t value)
{
    printf("[WiFi_Timing] Write int_status%d: $%08X\n", index, value);
    timers[index].int_status &= value & 0x1;
}

void WiFi_Timers::write_target(int index, uint32_t value)
{
    printf("[WiFi_Timing] Write target%d: $%08X\n", index, value);

    if (index == 4)
        value >>= 12;
    timers[index].target = value;
}

void WiFi_Timers::write_ctrl(int index, uint32_t value)
{
    printf("[WiFi_Timing] Write ctrl%d: $%08X\n", index, value);

    if (value & 0x1)
        timers[index].count = 0;
    timers[index].auto_restart = (value >> 1) & 0x1;
    timers[index].enabled = (value >> 2) & 0x1;
}
