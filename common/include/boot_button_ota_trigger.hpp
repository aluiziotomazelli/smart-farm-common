// common/include/boot_button_ota_trigger.hpp
#pragma once

#include "driver/gpio.h"

#include "interfaces/i_ota_trigger.hpp"

/**
 * @class BootButtonOtaTrigger
 * @brief Concrete implementation of IOtaTrigger using a physical GPIO button.
 *
 * Polls the configured GPIO pin. It is designed to trigger when the button
 * is pressed (connected to GND with internal pull-up enabled).
 */
class BootButtonOtaTrigger : public IOtaTrigger
{
public:
    /**
     * @brief Construct a BootButtonOtaTrigger.
     * @param gpio The GPIO pin number where the button is connected.
     */
    explicit BootButtonOtaTrigger(gpio_num_t gpio);

    /** @copydoc IOtaTrigger::init() */
    esp_err_t init() override;

    /** @copydoc IOtaTrigger::is_triggered() */
    bool is_triggered() override;

private:
    gpio_num_t gpio_;
};
