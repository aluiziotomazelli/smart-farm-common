#pragma once

#include <gmock/gmock.h>

#include "esp_err.h"

#include "interfaces/i_nvs_core.hpp"

class MockNvsCore : public INvsCore
{
public:
    MOCK_METHOD(esp_err_t, load_core, (CoreStorage & core), (override));
    MOCK_METHOD(esp_err_t, save_core, (const CoreStorage& core, bool force_nvs_commit), (override));
};
