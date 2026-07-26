#include "nvs_core.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_attr.h"
#include <cstring>
#include <cstddef>

static const char* TAG = "NvsCore";

RTC_DATA_ATTR static CoreStorage rtc_core_storage;
RTC_DATA_ATTR static bool rtc_core_valid = false;

NvsCore::NvsCore(const char* ns, idf_hals::INvsHAL& hal)
    : hal_(hal)
    , namespace_(ns)
{
    core_ = {};
}

uint32_t NvsCore::calculate_crc(const CoreStorage &storage)
{
    return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&storage), offsetof(CoreStorage, crc));
}

esp_err_t NvsCore::init_partition()
{
    esp_err_t err = hal_.flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition invalid, erasing");
        hal_.flash_erase();
        err = hal_.flash_init();
    }
    return err;
}

esp_err_t NvsCore::open_nvs(nvs_open_mode_t mode)
{
    if (is_open_)
        return ESP_OK; // Already open
    esp_err_t err = hal_.open(namespace_, mode, &handle_);
    if (err == ESP_OK)
        is_open_ = true;
    return err;
}

void NvsCore::close_nvs()
{
    if (is_open_) {
        hal_.close(handle_);
        is_open_ = false;
    }
}

esp_err_t NvsCore::load()
{
    esp_err_t err = open_nvs(NVS_READONLY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for reading: %s", esp_err_to_name(err));
        return err;
    }

    // 1. Try RTC first (fast, survives deep sleep)
    if (rtc_core_valid && rtc_core_storage.magic == CORE_STORAGE_MAGIC) {
        uint32_t expected_crc = calculate_crc(rtc_core_storage);
        if (rtc_core_storage.crc == expected_crc) {
            ESP_LOGI(TAG, "Loaded core data from RTC memory");
            core_ = rtc_core_storage;
            err = load_app_data();
            close_nvs();
            return err;
        }
        ESP_LOGW(TAG, "RTC core data CRC mismatch, falling back to NVS");
    }

    // 2. Fallback to NVS
    err = load_struct("core_data", core_);
    if (err == ESP_OK) {
        uint32_t expected_crc = calculate_crc(core_);
        if (core_.magic != CORE_STORAGE_MAGIC || core_.crc != expected_crc) {
            ESP_LOGW(TAG, "NVS core data invalid magic or CRC mismatch");
            err = ESP_ERR_NVS_INVALID_STATE;
        }
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Core data not found or corrupt: %s", esp_err_to_name(err));
        apply_core_defaults();
    } else {
        ESP_LOGI(TAG, "Loaded core data from NVS flash");
        if (core_.schema_version != CORE_SCHEMA_VERSION) {
            ESP_LOGW(TAG, "Schema mismatch (%u != %u). Migrating...", (unsigned int)core_.schema_version, (unsigned int)CORE_SCHEMA_VERSION);
            core_.schema_version = CORE_SCHEMA_VERSION;
        }
    }

    // Sync RTC with valid core_
    rtc_core_storage = core_;
    rtc_core_valid = true;

    err = load_app_data();
    close_nvs();
    return err;
}

esp_err_t NvsCore::save(bool force_nvs)
{
    // Update CRC
    core_.crc = calculate_crc(core_);

    bool is_dirty = !rtc_core_valid || (core_ != rtc_core_storage);
    bool need_nvs = force_nvs || is_dirty;

    // Always update RTC memory
    rtc_core_storage = core_;
    rtc_core_valid = true;

    esp_err_t err = open_nvs(NVS_READWRITE);
    if (err != ESP_OK) {
        return err;
    }

    if (need_nvs) {
        err = save_struct("core_data", core_);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Saved core data to NVS flash (dirty: %d, force: %d)", is_dirty, force_nvs);
        }
    }

    esp_err_t app_err = save_app_data(force_nvs);
    if (err == ESP_OK) {
        err = app_err;
    }

    if (is_open_) {
        if (err == ESP_OK) {
            err = hal_.commit(handle_);
        }
        close_nvs();
    }

    return err;
}

void NvsCore::apply_core_defaults()
{
    core_ = {};
    core_.magic = CORE_STORAGE_MAGIC;
    core_.schema_version = CORE_SCHEMA_VERSION;
    core_.node_type = farm::NodeType::UNKNOWN; 
    core_.node_id = farm::NodeId::UNKNOWN;
    core_.hw_revision = 1;
    core_.fw_major = 0;
    core_.fw_minor = 1;
    core_.fw_patch = 0;
    core_.boot_count = 0;
    core_.crash_count = 0;
    core_.has_valid_time = false;
    core_.unix_time = 0;
    core_.last_sync_uptime = 0;
    core_.power_profile = PowerProfile::ALWAYS_ON;
    core_.sleep_interval_s = 3600;
    core_.last_wake = WakeSource::POWER_ON;
    core_.crc = calculate_crc(core_);
}

void NvsCore::factory_reset()
{
    erase_namespace(); // Erase all first
    apply_core_defaults();
    set_app_defaults();
    save(true); // Force save default values to NVS
}

void NvsCore::invalidate_rtc_cache()
{
    rtc_core_valid = false;
    rtc_core_storage = {};
}

esp_err_t NvsCore::erase_namespace()
{
    invalidate_rtc_cache();
    esp_err_t err = open_nvs(NVS_READWRITE);
    if (err != ESP_OK)
        return err;

    err = hal_.erase_all(handle_);
    if (err == ESP_OK) {
        err = hal_.commit(handle_);
    }

    close_nvs();
    return err;
}