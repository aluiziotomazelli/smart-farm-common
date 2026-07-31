#pragma once

#include <gmock/gmock.h>

#include "esp_err.h"

#include "interfaces/i_nvs_core.hpp"

class MockNvsCore : public INvsCore
{
public:
    MOCK_METHOD(esp_err_t, load_core, (CoreStorage & core), (override));
    MOCK_METHOD(esp_err_t, save_core, (const CoreStorage& core, bool force_nvs_commit), (override));
    MOCK_METHOD(
        void,
        process_boot_reasons,
        (CoreStorage & core, esp_reset_reason_t reset_reason, esp_sleep_wakeup_cause_t wakeup_cause, bool& out_pending_commit),
        (override));
    MOCK_METHOD(esp_err_t, create_default_storage, (CoreStorage & core, const CoreStorage& default_core), (override));
};
