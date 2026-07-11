// components/smart-farm-common/common/include/interfaces/i_hal_timer.hpp
#pragma once

#include <cstdint>

namespace smart_farm {

/**
 * @brief Interface for system time services.
 */
class IHalTimer
{
public:
    virtual ~IHalTimer() = default;

    /** @copydoc esp_timer_get_time */
    virtual int64_t get_time_us() const = 0;
};

} // namespace smart_farm
