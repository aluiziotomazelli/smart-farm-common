#pragma once

#include "app_storage.hpp"
#include "core_types.hpp"
#include "interfaces/i_nvs_core.hpp"
#include "interfaces/i_persistence_backend.hpp"

/**
 * @class NvsCore
 * @brief Persistence manager for common Node CoreData.
 *
 * Inherits storage management (RTC/NVS fallback, CRC validation, dirty check)
 * from AppStorage<CoreData, CORE_MAGIC, CORE_VERSION>.
 */
class NvsCore : public INvsCore,
                public AppStorage<CoreData, CORE_MAGIC, CORE_VERSION>
{
public:
    NvsCore(IPersistenceBackend& rtc_core, IPersistenceBackend& nvs_core);
    virtual ~NvsCore() override = default;

    /** @copydoc INvsCore::init */
    esp_err_t init(CoreData& core, const CoreData& default_core) override
    {
        return init_app_data_impl(core, default_core);
    }

    /** @copydoc INvsCore::load_core */
    esp_err_t load_core(CoreData& core) override
    {
        return load_app_data_impl(core);
    }

    /** @copydoc INvsCore::save_core */
    esp_err_t save_core(const CoreData& core, bool force_nvs_commit = false) override
    {
        return save_app_data_impl(core, force_nvs_commit);
    }

    /** @copydoc INvsCore::process_boot_reasons */
    void process_boot_reasons(
        CoreData& core,
        esp_reset_reason_t reset_reason,
        esp_sleep_wakeup_cause_t wakeup_cause,
        bool& out_pending_commit) override;
};