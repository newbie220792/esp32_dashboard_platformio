#pragma once
#include <cstddef>

namespace Topics
{
    namespace Subscribe
    {
        static constexpr const char *PI_CPU = "pi/cpu";
        static constexpr const char *PI_MEM = "pi/mem";
        static constexpr const char *PI_TEMP = "pi/temp";
        static constexpr const char *PI_DISK = "pi/disk";
        static constexpr const char *WEATHER = "weather/description";
        static constexpr const char *TEMPERATURE = "weather/temperature";
        static constexpr const char *HUMIDITY = "weather/humidity";
        static constexpr const char *RAIN = "weather/rain";
        static constexpr const char *IS_DAY = "weather/is_day";

        static constexpr const char *ALL[] =
            {PI_CPU,
             PI_MEM,
             PI_TEMP,
             PI_DISK,
             WEATHER,
             TEMPERATURE,
             RAIN,
             IS_DAY,
             HUMIDITY};

        static constexpr size_t COUNT = sizeof(ALL) / sizeof(ALL[0]);
    };

    namespace Publish
    {
        static constexpr const char *HOME_LIGHT = "home/light";
        static constexpr const char *STATUS = "pi/status";
        static constexpr const char *COMMAND = "pi/command";

        static constexpr const char *ALL[] =
            {
                HOME_LIGHT,
                STATUS,
                COMMAND};

        static constexpr size_t COUNT =
            sizeof(ALL) / sizeof(ALL[0]);
    }
}