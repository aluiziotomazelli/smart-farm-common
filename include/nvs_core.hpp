#pragma once

#include <type_traits>

#include "esp_err.h"
#include "esp_rom_crc.h"

#include "interfaces/i_nvs_core.hpp"
#include "interfaces/i_hal_nvs.hpp"
#include "core_types.hpp"
#include "interfaces/i_persistence_backend.hpp"

/**
 * @class NvsCore
 * @brief Base class for NVS persistence management.
 *
 * This class provides a common interface and helper methods for saving and loading
 * data to/from NVS, while decoupling from the hardware via IHalNvs.
 */
class NvsCore : public virtual INvsCore
{
protected:
    /**
     * @brief Calculates the CRC of the given data.
     * @tparam T The type of the data to calculate the CRC of.
     * @param data The data to calculate the CRC of.
     * @return The CRC of the given data.
     *
     * @note: Compile-time validations:
     *          - T must be standard_layout (safe for offsetof)
     *          - T must have a crc field (not at offset 0)
     */
    template <typename T> uint32_t calculate_crc(const T& data)
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard_layout for safe offset calculation");
        static_assert(offsetof(T, crc) != 0, "T must have a non-first crc field");

        // Include from espnow_manager or define locally
        return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&data), offsetof(T, crc));
    };

public:
    /**
     * @brief Construct a new NvsCore object.
     * @param ns The NVS namespace to use.
     * @param hal Reference to the INvsHAL implementation.
     */
    // NvsCore(const char* ns, idf_hals::INvsHAL& hal);
    NvsCore(IPersistenceBackend& rtc_core, IPersistenceBackend& nvs_core);
    virtual ~NvsCore() override = default;

    esp_err_t load_core(CoreStorage& core) override;
    esp_err_t save_core(const CoreStorage& core, bool force_nvs_commit = false) override;

    esp_err_t process_boot_reasons(
        CoreStorage& core,
        esp_reset_reason_t reset_reason,
        esp_sleep_wakeup_cause_t wakeup_cause,
        bool& out_pending_commit) override;

    esp_err_t create_default_storage(CoreStorage& core, const CoreStorage& default_core) override;

private:
    IPersistenceBackend& rtc_core_;
    IPersistenceBackend& nvs_core_;

    esp_err_t load_raw_core(CoreStorage& core_out);
    esp_err_t validate_core_data(const CoreStorage& core);
    bool is_data_dirty(const CoreStorage& new_core) const;
};