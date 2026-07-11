// components/smart-farm-common/common/src/sm_hal_timer.cpp
#include "hal_timer.hpp"

#include "esp_timer.h"

namespace smart_farm {

int64_t SmHalTimer::get_time_us() const
{
    return esp_timer_get_time();
}

} // namespace smart_farm
