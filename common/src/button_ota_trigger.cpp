// common/src/button_ota_trigger.cpp

#include "button_ota_trigger.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "driver/gpio.h"

static const char* TAG = "ButtonOtaTrigger";

ButtonOtaTrigger::ButtonOtaTrigger(
    idf_hals::IGpioHAL& gpio,
    idf_hals::IHalFreertos& rtos,
    gpio_num_t pin,
    uint32_t hold_ms)
    : gpio_(gpio)
    , rtos_(rtos)
    , pin_(pin)
    , hold_ms_(hold_ms)
{
}

esp_err_t ButtonOtaTrigger::arm(IOtaTriggerListener& listener)
{
    listener_ = &listener;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    esp_err_t err = gpio_.config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", pin_, esp_err_to_name(err));
        return err;
    }

    BaseType_t res = rtos_.task_create(
        &ButtonOtaTrigger::poll_task,
        "ota_btn_trig",
        2048,
        this,
        5,
        &task_
    );
    return res == pdPASS ? ESP_OK : ESP_FAIL;
}

void ButtonOtaTrigger::disarm()
{
    if (task_) {
        rtos_.task_delete(task_);
        task_ = nullptr;
    }
    listener_ = nullptr;
}

void ButtonOtaTrigger::poll_task(void* arg)
{
    auto* self = static_cast<ButtonOtaTrigger*>(arg);
    uint32_t held_ms = 0;
    while (true) {
        if (self->gpio_.get_level(self->pin_) == 0) {
            held_ms += 100;
            if (held_ms >= self->hold_ms_) {
                if (self->listener_) {
                    self->listener_->on_ota_triggered(OtaTriggerSource::BUTTON);
                }
                held_ms = 0;
                self->rtos_.task_delay(pdMS_TO_TICKS(2000));
            }
        }
        else {
            held_ms = 0;
        }
        self->rtos_.task_delay(pdMS_TO_TICKS(100));
    }
}
