#pragma once

#include <gmock/gmock.h>

#include "esp_err.h"

#include "interfaces/i_nvs_core.hpp"

class MockNvsCore : public INvsCore
{
public:
    MOCK_METHOD(esp_err_t, init, (CoreData & core, const CoreData& default_core), (override));
    MOCK_METHOD(esp_err_t, load_core, (CoreData & core), (override));
    MOCK_METHOD(esp_err_t, save_core, (const CoreData& core, bool force_nvs_commit), (override));
    MOCK_METHOD(
        void,
        process_boot_reasons,
        (CoreData & core, esp_reset_reason_t reset_reason, esp_sleep_wakeup_cause_t wakeup_cause, bool& out_pending_commit),
        (override));
};
