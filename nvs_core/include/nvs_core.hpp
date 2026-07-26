#pragma once

#include "interfaces/i_nvs_core.hpp"
#include "interfaces/i_hal_nvs.hpp"
#include "core_types.hpp"
#include "esp_err.h"

/**
 * @class NvsCore
 * @brief Base class for NVS persistence management.
 *
 * This class provides a common interface and helper methods for saving and loading
 * data to/from NVS, while decoupling from the hardware via IHalNvs.
 */
class NvsCore : public INvsCore
{
protected:
    idf_hals::INvsHAL &hal_; ///< Reference to the NVS Hardware Abstraction Layer.
    CoreStorage core_;

    // NVS handle (kept open during load/commit for efficiency)
    nvs_handle_t handle_ = 0;
    bool         is_open_ = false;
    const char  *namespace_;

    // Helpers for derived classes
    template <typename T> esp_err_t save_struct(const char *key, const T &data)
    {
        if (!is_open_)
            return ESP_FAIL;
        return hal_.set_blob(handle_, key, &data, sizeof(T));
    }

    template <typename T> esp_err_t load_struct(const char *key, T &data)
    {
        if (!is_open_)
            return ESP_FAIL;
        size_t required_size = sizeof(T);
        // Attempt to read. Return error if size mismatch or not found.
        esp_err_t err = hal_.get_blob(handle_, key, &data, &required_size);
        if (err == ESP_OK && required_size != sizeof(T))
            return ESP_ERR_NVS_INVALID_LENGTH;
        return err;
    }

    // Pure virtual methods to be implemented by the application
    virtual esp_err_t load_app_data()                     = 0;
    virtual esp_err_t save_app_data(bool force_nvs = false) = 0;
    virtual void      set_app_defaults()                = 0;

private:
    void      apply_core_defaults();
    esp_err_t open_nvs(nvs_open_mode_t mode);
    void      close_nvs();
    uint32_t  calculate_crc(const CoreStorage &storage);

public:
    /**
     * @brief Construct a new NvsCore object.
     * @param ns The NVS namespace to use.
     * @param hal Reference to the INvsHAL implementation.
     */
    NvsCore(const char *ns, idf_hals::INvsHAL &hal);
    virtual ~NvsCore() override = default;

    // Initialize partition
    esp_err_t init_partition() override;

    // Master flow: Load Core + App (from RTC with NVS fallback)
    esp_err_t load() override;

    // Master flow: Save Core + App (RTC first, NVS if dirty or force_nvs)
    esp_err_t save(bool force_nvs = false) override;

    // Access to common data
    CoreStorage &get_core_data()
    {
        return core_;
    }

    const CoreStorage &get_core_data() const
    {
        return core_;
    }

    // Complete factory reset (erases only the namespace)
    void factory_reset() override;

    // Erases everything in the namespace
    esp_err_t erase_namespace() override;

    // Invalidates the in-memory RTC cache
    void invalidate_rtc_cache();

public:
    template <typename T> esp_err_t load_struct_public(const char *key, T &data)
    {
        return load_struct(key, data);
    }

    template <typename T> esp_err_t save_struct_public(const char *key, const T &data)
    {
        return save_struct(key, data);
    }
};