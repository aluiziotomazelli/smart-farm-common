#pragma once

#include "esp_err.h"

/**
 * @brief Interface for the NVS Core storage manager.
 * 
 * Provides a standardized way to initialize, load, and commit system and 
 * application data to Non-Volatile Storage.
 */
class INvsCore {
public:
    virtual ~INvsCore() = default;

    /**
     * @brief Initializes the NVS partition.
     */
    virtual esp_err_t init_partition() = 0;

    /**
     * @brief Loads core and application data from NVS.
     */
    virtual esp_err_t load() = 0;

    /**
     * @brief Saves data to RTC memory, and optionally commits to NVS flash if force_nvs is true or data is dirty.
     * @param force_nvs Force write to NVS flash regardless of dirty state.
     */
    virtual esp_err_t save(bool force_nvs = false) = 0;

    /**
     * @brief Resets the namespace to default values.
     */
    virtual void factory_reset() = 0;

    /**
     * @brief Erases the entire namespace.
     */
    virtual esp_err_t erase_namespace() = 0;
};
