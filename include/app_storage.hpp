#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

#include "esp_err.h"
#include "esp_rom_crc.h"

#include "esp_log.h"

#include "interfaces/i_persistence_backend.hpp"

/**
 * @brief Base template providing RTC/NVS storage, CRC validation,
 *        dirty checks, and initialization fallback for application-specific structs.
 */
template <typename T> class AppStorage
{
    static_assert(std::is_standard_layout_v<T>, "AppStorage<T>: T must be standard_layout");
    static_assert(offsetof(T, crc) != 0, "AppStorage<T>: 'crc' must not be the first field of T");

public:
    /**
     * @brief Initializes the AppStorage object.
     * @param rtc_stats Reference to the RTC persistence backend.
     * @param nvs_stats Reference to the NVS persistence backend.
     */
    AppStorage(IPersistenceBackend& rtc, IPersistenceBackend& nvs, const char* tag)
        : rtc_(rtc)
        , nvs_(nvs)
        , tag_(tag)
    {
    }

    virtual ~AppStorage() = default;

protected:
    /**
     * @brief Initializes application storage. Loads existing data or creates default storage.
     * @param[out] out_data Populated with loaded or default data.
     * @param[in] default_data Fallback data to persist if load fails.
     * @return ESP_OK on success, error code on persistence failure.
     */
    esp_err_t init_app_data_impl(T& out_data, const T& default_data)
    {
        esp_err_t ret = load_app_data_impl(out_data);
        if (ret == ESP_OK) {
            ESP_LOGI(tag_, "Loaded app data from storage");
            return ESP_OK;
        }

        ESP_LOGW(tag_, "App storage load failed (%s), recreating default storage", esp_err_to_name(ret));
        out_data = default_data;
        ret = save_app_data_impl(out_data, /*force_nvs_commit=*/true);
        if (ret != ESP_OK) {
            ESP_LOGE(tag_, "Failed to create default app storage: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(tag_, "Created default app storage");
        return ESP_OK;
    }

    /**
     * @brief Loads application data, checking RTC first and falling back to NVS.
     */
    esp_err_t load_app_data_impl(T& out)
    {
        T temp{};
        esp_err_t ret = load_raw(temp);
        if (ret == ESP_OK) {
            out = temp;
        }
        return ret;
    }

    /**
     * @brief Persists application data to RTC (if dirty) and optionally to NVS.
     */
    esp_err_t save_app_data_impl(const T& data, bool force_nvs_commit = false)
    {
        T stamped = data;
        stamped.magic = T::MAGIC;
        stamped.version = T::VERSION;
        stamped.crc = calculate_crc(stamped);

        const bool dirty = is_dirty(stamped);

        if (!dirty && !force_nvs_commit) {
            return ESP_OK;
        }

        if (dirty) {
            rtc_.save(&stamped, sizeof(stamped));
            ESP_LOGD(tag_, "Saved app data to RTC");
        }

        if (force_nvs_commit) {
            esp_err_t err = nvs_.save(&stamped, sizeof(stamped));
            if (err != ESP_OK) {
                ESP_LOGE(tag_, "Failed to save app data to NVS: %s", esp_err_to_name(err));
                return err;
            }
            ESP_LOGI(tag_, "Saved app data to NVS");
        }
        return ESP_OK;
    }

private:
    IPersistenceBackend& rtc_;
    IPersistenceBackend& nvs_;
    const char* tag_;

    uint32_t calculate_crc(const T& data) const
    {
        return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&data), offsetof(T, crc));
    }

    esp_err_t load_raw(T& out)
    {
        // 1. Try RTC first (fast, survives deep-sleep)
        if (rtc_.load(&out, sizeof(T)) == ESP_OK) {
            if (validate(out) == ESP_OK) {
                ESP_LOGD(tag_, "Loaded app data from RTC memory");
                return ESP_OK;
            }
        }
        // 2. Fall back to NVS
        if (nvs_.load(&out, sizeof(T)) == ESP_OK) {
            if (validate(out) == ESP_OK) {
                rtc_.save(&out, sizeof(T)); // Sync valid NVS data back to RTC
                ESP_LOGD(tag_, "Loaded app data from NVS flash");
                return ESP_OK;
            }
        }
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t validate(const T& data) const
    {
        if (data.magic != T::MAGIC) {
            ESP_LOGW(tag_, "Invalid magic: 0x%X (expected 0x%X)", (unsigned)data.magic, (unsigned)T::MAGIC);
            return ESP_ERR_INVALID_ARG;
        }
        if (data.version != T::VERSION) {
            ESP_LOGW(tag_, "Version mismatch: %d (expected %d)", (int)data.version, (int)T::VERSION);
            return ESP_ERR_INVALID_VERSION;
        }
        if (data.crc != calculate_crc(data)) {
            ESP_LOGW(tag_, "CRC mismatch in app data");
            return ESP_ERR_INVALID_CRC;
        }
        return ESP_OK;
    }

    bool is_dirty(const T& new_data) const
    {
        T current{};
        if (rtc_.load(&current, sizeof(T)) != ESP_OK) {
            return true; // assume dirty if unreadable
        }
        return current != new_data;
    }
};