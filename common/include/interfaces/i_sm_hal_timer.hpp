// components/smart-farm-common/common/include/interfaces/i_sm_hal_timer.hpp
#pragma once

#include <cstdint>

namespace smart_farm {

/**
 * @brief Interface for system time services.
 */
class ISmHalTimer {
public:
    virtual ~ISmHalTimer() = default;

    /** @copydoc esp_timer_get_time */
    virtual int64_t get_time_us() const = 0;
};

} // namespace smart_farm
