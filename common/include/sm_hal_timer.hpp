// components/smart-farm-common/common/include/sm_hal_timer.hpp
#pragma once

#include "interfaces/i_sm_hal_timer.hpp"

namespace smart_farm {

/**
 * @brief Implementation of ISmHalTimer.
 */
class SmHalTimer : public ISmHalTimer {
public:
    int64_t get_time_us() const override;
};

} // namespace smart_farm
