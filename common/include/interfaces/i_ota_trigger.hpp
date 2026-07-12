// common/include/interfaces/i_ota_trigger.hpp
#pragma once

#include "esp_err.h"

/**
 * @brief Sources that can trigger an OTA update.
 */
enum class OtaTriggerSource
{
    BUTTON,  ///< Hardware boot button trigger source
    ESPNOW   ///< ESP-NOW networking command trigger source
};

/**
 * @brief Interface for listening to OTA trigger events.
 */
class IOtaTriggerListener
{
public:
    virtual ~IOtaTriggerListener() = default;

    /**
     * @brief Callback invoked when an OTA update is triggered.
     *
     * @param source The source that triggered the OTA update.
     */
    virtual void on_ota_triggered(OtaTriggerSource source) = 0;
};

/**
 * @brief Interface representing an OTA update trigger mechanism.
 */
class IOtaTrigger
{
public:
    virtual ~IOtaTrigger() = default;

    /**
     * @brief Arm/register the listener for OTA trigger events.
     *
     * @param listener The listener callback implementation.
     * @return ESP_OK: on success
     * @return ESP_FAIL: if trigger cannot be armed
     */
    virtual esp_err_t arm(IOtaTriggerListener& listener) = 0;

    /**
     * @brief Disarm/unregister the listener.
     */
    virtual void disarm() = 0;
};
