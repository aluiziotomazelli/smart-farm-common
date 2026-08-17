#pragma once

#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_system.h"

#include "core_types.hpp"

/**
 * @brief Interface for the NVS Core storage manager.
 *
 * Provides a standardized way to initialize, load, and commit system
 * core data to Non-Volatile Storage, and to process hardware reset/boot reasons.
 */
class INvsCore
{
public:
    virtual ~INvsCore() = default;

    /**
     * @brief Initializes core storage. Loads existing core data or writes default_core if load fails.
     * @param[out] core Populated with loaded or default core data.
     * @param[in] default_core Fallback core data to persist if storage is empty/invalid.
     * @return ESP_OK on success, or an error code.
     */
    virtual esp_err_t init(CoreData& core, const CoreData& default_core) = 0;

    /**
     * @brief Loads the core node data from storage (RTC or NVS).
     * @param[out] core Populated with loaded data.
     * @return ESP_OK on success, or an error code.
     */
    virtual esp_err_t load_core(CoreData& core) = 0;

    /**
     * @brief Persists core node data to RTC (if dirty) and optionally to NVS.
     * @param[in] core The core data to save.
     * @param[in] force_nvs_commit If true, also commits data to NVS flash.
     * @return ESP_OK on success, or an error code.
     */
    virtual esp_err_t save_core(const CoreData& core, bool force_nvs_commit = false) = 0;

    /**
     * @brief Analyzes hardware reset reason and wakeup cause, updating boot/crash counters and wake source.
     * @param[in,out] core The core data to update.
     * @param[in] reset_reason ESP hardware reset reason.
     * @param[in] wakeup_cause ESP deep sleep wakeup cause.
     * @param[out] out_pending_commit Set to true if a crash occurred, requiring immediate NVS commit.
     */
    virtual void process_boot_reasons(
        CoreData& core,
        esp_reset_reason_t reset_reason,
        esp_sleep_wakeup_cause_t wakeup_cause,
        bool& out_pending_commit) = 0;
};
