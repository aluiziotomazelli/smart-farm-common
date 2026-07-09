// common/include/interfaces/i_ota_trigger.hpp
#pragma once

#include "esp_err.h"

/**
 * @class IOtaTrigger
 * @brief Abstract interface for OTA trigger sources.
 *
 * Implementations can be physical buttons, ESP-NOW command queues, or other network protocols.
 * The orchestrator polls this interface to decide when to initiate the OTA firmware update.
 */
class IOtaTrigger
{
public:
    virtual ~IOtaTrigger() = default;

    /**
     * @brief Initialize the trigger hardware or software source.
     *
     * @return
     *     - ESP_OK: Initialization successful.
     *     - Other: Error code from the underlying driver or peripheral setup.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Check if an OTA trigger event has occurred.
     *
     * This method is polled by the controller. Implementations should handle debouncing
     * or latching/clearing events as appropriate.
     *
     * @return
     *     - true: An OTA update has been requested.
     *     - false: No trigger event detected.
     */
    virtual bool is_triggered() = 0;
};
