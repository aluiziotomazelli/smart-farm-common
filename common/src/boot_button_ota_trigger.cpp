// common/src/boot_button_ota_trigger.cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "boot_button_ota_trigger.hpp"

BootButtonOtaTrigger::BootButtonOtaTrigger(gpio_num_t gpio)
    : gpio_(gpio)
{
}

/** @copydoc IOtaTrigger::init() */
esp_err_t BootButtonOtaTrigger::init()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << gpio_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    return gpio_config(&io_conf);
}

/** @copydoc IOtaTrigger::is_triggered() */
bool BootButtonOtaTrigger::is_triggered()
{
    if (gpio_get_level(gpio_) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (gpio_get_level(gpio_) == 0) {
            return true;
        }
    }
    return false;
}
