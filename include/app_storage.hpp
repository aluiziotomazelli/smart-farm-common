#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_crc.h"

#include "interfaces/i_persistence_backend.hpp"

/**
 * @struct StorageEnvelope
 * @brief Storage envelope wrapper that encapsulates header metadata (magic, version),
 *        the pure domain payload, and a CRC32 integrity trailer.
 *
 * @tparam TData Domain data struct (must be standard_layout).
 * @tparam Magic Unique 32-bit schema identifier.
 * @tparam Version 8-bit schema version number.
 */
template <typename TData, uint32_t Magic, uint8_t Version>
struct StorageEnvelope
{
    static_assert(std::is_standard_layout_v<TData>, "TData must be standard_layout");

    static constexpr uint32_t MAGIC = Magic;
    static constexpr uint8_t VERSION = Version;

    uint32_t magic = MAGIC;
    uint8_t version = VERSION;
    uint8_t reserved[3] = {0}; // Explicit 4-byte boundary padding

    TData data{};

    uint32_t crc = 0; // Guaranteed to be the last field

    void reset()
    {
        *this = {};
        magic = MAGIC;
        version = VERSION;
    }

    bool operator==(const StorageEnvelope& other) const
    {
        return magic == other.magic && version == other.version && data == other.data && crc == other.crc;
    }

    bool operator!=(const StorageEnvelope& other) const { return !(*this == other); }
};

/**
 * @class AppStorage
 * @brief Base template providing RTC/NVS storage, CRC validation,
 *        dirty checks, and initialization fallback for application structs.
 *
 * Automatically wraps TData in a StorageEnvelope<TData, Magic, Version> so that the
 * application logic only interacts with clean domain data types.
 *
 * @tparam TData Domain data struct.
 * @tparam Magic Unique 32-bit schema identifier.
 * @tparam Version 8-bit schema version number.
 */
template <typename TData, uint32_t Magic, uint8_t Version>
class AppStorage
{
public:
    using Envelope = StorageEnvelope<TData, Magic, Version>;

    static_assert(std::is_standard_layout_v<Envelope>, "Envelope must be standard_layout");
    static_assert(offsetof(Envelope, crc) != 0, "'crc' must not be the first field of Envelope");

    /**
     * @brief Initializes the AppStorage object.
     * @param rtc Reference to the RTC persistence backend.
     * @param nvs Reference to the NVS persistence backend.
     * @param tag Logging tag prefix.
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
     * @brief Initializes application storage. Loads existing data or persists default data.
     * @param[out] out_data Populated with loaded or default data.
     * @param[in] default_data Fallback data to persist if load fails.
     * @return ESP_OK on success, error code on persistence failure.
     */
    esp_err_t init_app_data_impl(TData& out_data, const TData& default_data)
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
     * @param[out] out Populated with loaded domain data on success.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t load_app_data_impl(TData& out)
    {
        Envelope env{};
        esp_err_t ret = load_raw(env);
        if (ret == ESP_OK) {
            out = env.data;
        }
        return ret;
    }

    /**
     * @brief Persists application data to RTC (if dirty) and optionally to NVS.
     * @param[in] data Domain data to persist.
     * @param[in] force_nvs_commit If true, forces a commit to NVS flash.
     * @return ESP_OK on success, error code on persistence failure.
     */
    esp_err_t save_app_data_impl(const TData& data, bool force_nvs_commit = false)
    {
        Envelope env{};
        env.magic = Magic;
        env.version = Version;
        env.data = data;
        env.crc = calculate_crc(env);

        const bool dirty = is_dirty(env);

        if (!dirty && !force_nvs_commit) {
            return ESP_OK;
        }

        if (dirty) {
            rtc_.save(&env, sizeof(Envelope));
            ESP_LOGD(tag_, "Saved app data to RTC");
        }

        if (force_nvs_commit) {
            esp_err_t err = nvs_.save(&env, sizeof(Envelope));
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

    uint32_t calculate_crc(const Envelope& env) const
    {
        return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&env), offsetof(Envelope, crc));
    }

    esp_err_t load_raw(Envelope& out)
    {
        // 1. Try RTC first (fast, survives deep-sleep)
        if (rtc_.load(&out, sizeof(Envelope)) == ESP_OK) {
            if (validate(out) == ESP_OK) {
                ESP_LOGD(tag_, "Loaded app data from RTC memory");
                return ESP_OK;
            }
        }
        // 2. Fall back to NVS
        if (nvs_.load(&out, sizeof(Envelope)) == ESP_OK) {
            if (validate(out) == ESP_OK) {
                rtc_.save(&out, sizeof(Envelope)); // Sync valid NVS data back to RTC
                ESP_LOGD(tag_, "Loaded app data from NVS flash");
                return ESP_OK;
            }
        }
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t validate(const Envelope& env) const
    {
        if (env.magic != Magic) {
            ESP_LOGW(tag_, "Invalid magic: 0x%X (expected 0x%X)", (unsigned)env.magic, (unsigned)Magic);
            return ESP_ERR_INVALID_ARG;
        }
        if (env.version != Version) {
            ESP_LOGW(tag_, "Version mismatch: %d (expected %d)", (int)env.version, (int)Version);
            return ESP_ERR_INVALID_VERSION;
        }
        if (env.crc != calculate_crc(env)) {
            ESP_LOGW(tag_, "CRC mismatch in app data");
            return ESP_ERR_INVALID_CRC;
        }
        return ESP_OK;
    }

    bool is_dirty(const Envelope& new_env) const
    {
        Envelope current_env{};
        if (rtc_.load(&current_env, sizeof(Envelope)) != ESP_OK) {
            return true; // Assume dirty if unreadable
        }
        return current_env.data != new_env.data; // Compares domain data using TData::operator!=
    }
};