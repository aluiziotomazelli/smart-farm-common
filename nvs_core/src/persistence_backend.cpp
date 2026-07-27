// nvs_core/src/persistence_backend.cpp
#include "persistence_backend.hpp"

#include "esp_log.h"

static const char* TAG = "NvsCore PersistenceBackend";
static const char* NVS_NAMESPACE = "nvs_core";

// --- RTC Backend ---
RtcBackend::RtcBackend(void* storage, size_t size)
    : storage_(storage)
    , size_(size)
{
}

esp_err_t RtcBackend::load(void* data, size_t size)
{
    if (size > size_)
        return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(data, storage_, size);
    return ESP_OK;
}

esp_err_t RtcBackend::save(const void* data, size_t size)
{
    if (size > size_)
        return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(storage_, data, size);
    return ESP_OK;
}

// --- NVS Backend ---
NvsBackend::NvsBackend(idf_hals::INvsHAL& nvs_hal, const char* nvs_key)
    : nvs_(nvs_hal)
    , nvs_key_(nvs_key)
{
}

esp_err_t NvsBackend::load(void* data, size_t size)
{
    esp_err_t err = init_nvs();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_.open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t actual_size = size;
    err = nvs_.get_blob(handle, nvs_key_, data, &actual_size);
    nvs_.close(handle);

    if (err != ESP_OK) {
        return err;
    }
    if (actual_size != size) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

esp_err_t NvsBackend::save(const void* data, size_t size)
{
    esp_err_t err = init_nvs();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_.open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_.set_blob(handle, nvs_key_, data, size);
    if (err == ESP_OK) {
        err = nvs_.commit(handle);
    }
    nvs_.close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save data to NVS: 0x%x", err);
    }

    return err;
}

esp_err_t NvsBackend::init_nvs()
{
    if (nvs_initialized_) {
        return ESP_OK;
    }

    esp_err_t err;

    err = nvs_.flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_.flash_erase();
        err = nvs_.flash_init();
    }
    if (err == ESP_OK) {
        nvs_initialized_ = true;
    }
    return err;
}