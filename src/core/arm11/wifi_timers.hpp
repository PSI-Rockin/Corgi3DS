#ifndef WIFI_TIMERS_HPP
#define WIFI_TIMERS_HPP
#include <cstdint>
#include <functional>

struct WiFi_Timer
{
    uint32_t target;
    uint32_t count;
    bool auto_restart;
    bool int_status;
    bool enabled;
};

class WiFi_Timers
{
    private:
        //Four low-frequency timers and one high-frequency timer
        WiFi_Timer timers[5];

        std::function<void(int id)> send_soc_irq;
    public:
        WiFi_Timers();

        void reset();
        void run(int cycles);
        void set_send_soc_irq(std::function<void(int id)> func);

        uint32_t read_target(int index);
        uint32_t read_count(int index);
        uint32_t read_ctrl(int index);
        uint32_t read_int_status(int index);

        void write_target(int index, uint32_t value);
        void write_ctrl(int index, uint32_t value);
        void write_int_status(int index, uint32_t value);
};

#endif // WIFI_TIMERS_HPP
