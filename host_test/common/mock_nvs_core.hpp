#pragma once

#include <gmock/gmock.h>

#include "esp_err.h"

#include "interfaces/i_nvs_core.hpp"

class MockNvsCore : public INvsCore
{
public:
    MOCK_METHOD(esp_err_t, init_partition, (), (override));
    MOCK_METHOD(esp_err_t, load, (), (override));
    MOCK_METHOD(esp_err_t, save, (bool force_nvs), (override));
    MOCK_METHOD(void, factory_reset, (), (override));
    MOCK_METHOD(esp_err_t, erase_namespace, (), (override));
};
