// common/include/espnow_ota_trigger.hpp
#pragma once

#include "interfaces/i_ota_trigger.hpp"

/**
 * @class EspNowOtaTrigger
 * @brief Network-based trigger implementation for OTA updates over ESP-NOW.
 */
class EspNowOtaTrigger : public IOtaTrigger
{
public:
    /** @copydoc IOtaTrigger::arm */
    esp_err_t arm(IOtaTriggerListener& listener) override;

    /** @copydoc IOtaTrigger::disarm */
    void disarm() override;

    /**
     * @brief Manually notify the listener to trigger an OTA update.
     */
    void notify();

private:
    IOtaTriggerListener* listener_ = nullptr;
};
