// common/include/button_ota_trigger.hpp
#pragma once

#include "interfaces/i_ota_trigger.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_freertos.hpp"

#include "driver/gpio.h"

/**
 * @class ButtonOtaTrigger
 * @brief Hardware button-based trigger implementation for OTA updates.
 *
 * Uses a polling task to monitor a GPIO pin for a long-press duration.
 */
class ButtonOtaTrigger : public IOtaTrigger
{
public:
    /**
     * @brief Construct a new Button Ota Trigger object.
     *
     * @param gpio Reference to the GPIO HAL abstraction.
     * @param rtos Reference to the FreeRTOS HAL abstraction.
     * @param pin The GPIO pin number to poll.
     * @param hold_ms The long-press duration required to trigger OTA (default 3000ms).
     */
    ButtonOtaTrigger(
        idf_hals::IGpioHAL& gpio,
        idf_hals::IHalFreertos& rtos,
        gpio_num_t pin,
        uint32_t hold_ms = 3000);

    /** @copydoc IOtaTrigger::arm */
    esp_err_t arm(IOtaTriggerListener& listener) override;

    /** @copydoc IOtaTrigger::disarm */
    void disarm() override;

private:
    /**
     * @brief Private worker task function to poll the GPIO button.
     *
     * @param arg Pointer to the ButtonOtaTrigger instance.
     */
    static void poll_task(void* arg);

    idf_hals::IGpioHAL& gpio_;
    idf_hals::IHalFreertos& rtos_;
    gpio_num_t pin_;
    uint32_t hold_ms_;
    IOtaTriggerListener* listener_ = nullptr;
    TaskHandle_t task_ = nullptr;
};
