#include "nvs_core.hpp"
#include "core_types.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_attr.h"
#include <cstring>
#include <cstddef>

static const char* TAG = "NvsCore";

NvsCore::NvsCore(IPersistenceBackend& rtc_core, IPersistenceBackend& nvs_core)
    : rtc_core_(rtc_core)
    , nvs_core_(nvs_core)
{
}

esp_err_t NvsCore::load_core(CoreStorage& core)
{
    CoreStorage temp_core = {};

    esp_err_t ret = load_raw_core(temp_core);
    if (ret == ESP_OK) {
        core = temp_core;
    }

    return ret;
}

esp_err_t NvsCore::save_core(const CoreStorage& core, bool force_nvs_commit)
{
    CoreStorage new_core = core;
    new_core.magic = CoreStorage::CORE_MAGIC;
    new_core.version = CoreStorage::CORE_VERSION;

    bool is_dirty = is_data_dirty(new_core);

    // Calculate CRC to save if data is dirty
    new_core.crc = calculate_crc(new_core);

    // If data is not dirty and force_nvs_commit is false, return
    if (!is_dirty && !force_nvs_commit) {
        return ESP_OK;
    }

    // If is dirty, save to RTC
    if (is_dirty) {
        rtc_core_.save(&new_core, sizeof(new_core));
        ESP_LOGI(TAG, "Saved data to RTC");
    }

    // If NVS commit is forced, save to nvs
    if (force_nvs_commit) {
        esp_err_t err = nvs_core_.save(&new_core, sizeof(new_core));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Saved data to NVS");
            return ESP_OK;
        }
        else {
            ESP_LOGE(TAG, "Failed to save data to NVS: %s", esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

// =================================================
// Private methods
// =================================================

esp_err_t NvsCore::load_raw_core(CoreStorage& out)
{
    esp_err_t ret;

    // 1. Try RTC first (fast, survives deep-sleep)
    ret = rtc_core_.load(&out, sizeof(CoreStorage));
    if (ret == ESP_OK) {
        ret = validate_core_data(out);
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "Loaded core data from RTC memory");
            return ESP_OK;
        }
    }

    // 2. Fall back to NVS
    ret = nvs_core_.load(&out, sizeof(CoreStorage));
    if (ret == ESP_OK) {
        ret = validate_core_data(out);
        if (ret == ESP_OK) {
            // Sync valid NVS data back to RTC
            rtc_core_.save(&out, sizeof(CoreStorage));
            ESP_LOGD(TAG, "Loaded core data from NVS flash");
            return ESP_OK;
        }
    }
    return ret;
}

esp_err_t NvsCore::validate_core_data(const CoreStorage& core)
{
    if (core.magic != CoreStorage::CORE_MAGIC) {
        ESP_LOGW(TAG, "Magic mismatch: 0x%08X", core.magic);
        return ESP_ERR_INVALID_STATE;
    }

    if (core.version != CoreStorage::CORE_VERSION) {
        ESP_LOGW(TAG, "Version mismatch: %d", core.version);
        return ESP_ERR_INVALID_VERSION;
    }

    if (core.crc != calculate_crc<CoreStorage>(core)) {
        ESP_LOGW(TAG, "CRC mismatch: 0x%08X", core.crc);
        return ESP_ERR_INVALID_CRC;
    }
    return ESP_OK;
}

bool NvsCore::is_data_dirty(const CoreStorage& new_core) const
{
    CoreStorage current_core;

    // If we can't load from RTC, assume it's dirty to be safe
    if (rtc_core_.load(&current_core, sizeof(CoreStorage)) != ESP_OK) {
        return true;
    }

    return (current_core != new_core);
}