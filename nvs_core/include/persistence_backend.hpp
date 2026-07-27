// nvs_core/include/persistence_backend.hpp
#pragma once

#include "interfaces/i_persistence_backend.hpp"
#include "interfaces/i_hal_nvs.hpp"

/**
 * @brief Default RTC backend that uses a static PersistentData variable.
 * On real hardware, this variable is placed in RTC slow memory.
 */
class RtcBackend : public IPersistenceBackend
{
public:
    RtcBackend(void* storage, size_t size);

    /** @copydoc IPersistenceBackend::load */
    esp_err_t load(void* data, size_t size) override;

    /** @copydoc IPersistenceBackend::save */
    esp_err_t save(const void* data, size_t size) override;

private:
    void* storage_;
    size_t size_;
};

/**
 * @brief Default NVS backend that uses the nvs_flash component.
 */
class NvsBackend : public IPersistenceBackend
{
public:
    NvsBackend(idf_hals::INvsHAL& nvs_hal, const char* nvs_key);

    /** @copydoc IPersistenceBackend::load */
    esp_err_t load(void* data, size_t size) override;

    /** @copydoc IPersistenceBackend::save */
    esp_err_t save(const void* data, size_t size) override;

private:
    esp_err_t init_nvs();

    idf_hals::INvsHAL& nvs_;
    const char* nvs_key_;

    bool nvs_initialized_ = false;
};