// components/smart-farm-common/host_test/common/mock_i_hal_timer.hpp
#pragma once

#include <gmock/gmock.h>
#include "interfaces/i_hal_timer.hpp"

namespace smart_farm {

/**
 * @brief Mock implementation of ISmHalTimer.
 */
class MockSmHalTimer : public ISmHalTimer
{
public:
    MOCK_METHOD(int64_t, get_time_us, (), (const, override));
};

} // namespace smart_farm
