#pragma once

#include "esp_err.h"
#include "esp_system.h"
#include "esp_sleep.h"

#include "core_types.hpp"

/**
 * @brief Interface for the NVS Core storage manager.
 *
 * Provides a standardized way to initialize, load, and commit system and
 * application data to Non-Volatile Storage.
 */
class INvsCore
{
public:
    virtual ~INvsCore() = default;

    virtual esp_err_t init(
        CoreStorage& core,
        const CoreStorage& default_core,
        esp_reset_reason_t reset_reason,
        esp_sleep_wakeup_cause_t wakeup_cause,
        bool& out_pending_commit) = 0;

    virtual esp_err_t load_core(CoreStorage& core) = 0;
    virtual esp_err_t save_core(const CoreStorage& core, bool force_nvs_commit = false) = 0;
};
