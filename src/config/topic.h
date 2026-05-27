#pragma once
#include <cstddef>

namespace Topics
{
    namespace Subscribe
    {
        static constexpr const char *PI_CPU = "pi/cpu";
        static constexpr const char *PI_MEM = "pi/mem";
        static constexpr const char *PI_TEMP = "pi/temp";
        static constexpr const char *SYS_TIME = "sys/time";

        static constexpr const char *ALL[] =
            {PI_CPU,
             PI_MEM,
             PI_TEMP,
             SYS_TIME};

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