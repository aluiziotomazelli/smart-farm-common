// common/include/button_ota_trigger.hpp
#pragma once

#include "interfaces/i_ota_trigger.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_freertos.hpp"

#include "driver/gpio.h"

class ButtonOtaTrigger : public IOtaTrigger
{
public:
    ButtonOtaTrigger(
        idf_hals::IGpioHAL& gpio,
        idf_hals::IHalFreertos& rtos,
        gpio_num_t pin,
        uint32_t hold_ms = 3000);

    esp_err_t arm(IOtaTriggerListener& listener) override;
    void disarm() override;

private:
    static void poll_task(void* arg);

    idf_hals::IGpioHAL& gpio_;
    idf_hals::IHalFreertos& rtos_;
    gpio_num_t pin_;
    uint32_t hold_ms_;
    IOtaTriggerListener* listener_ = nullptr;
    TaskHandle_t task_ = nullptr;
};
