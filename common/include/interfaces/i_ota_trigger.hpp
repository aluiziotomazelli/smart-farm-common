// common/include/interfaces/i_ota_trigger.hpp
#pragma once

#include "esp_err.h"

enum class OtaTriggerSource
{
    BUTTON,
    ESPNOW
};

class IOtaTriggerListener
{
public:
    virtual void on_ota_triggered(OtaTriggerSource source) = 0;
    virtual ~IOtaTriggerListener() = default;
};

class IOtaTrigger
{
public:
    virtual ~IOtaTrigger() = default;

    /**
     * @brief Arm/register the listener for OTA trigger events.
     *
     * @param listener The listener callback implementation.
     * @return ESP_OK on success, error code otherwise.
     */
    virtual esp_err_t arm(IOtaTriggerListener& listener) = 0;

    /**
     * @brief Disarm/unregister the listener.
     */
    virtual void disarm() = 0;
};
