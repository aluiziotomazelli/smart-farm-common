// common/src/espnow_ota_trigger.cpp

#include "espnow_ota_trigger.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char* TAG = "EspNowOtaTrigger";

esp_err_t EspNowOtaTrigger::arm(IOtaTriggerListener& listener)
{
    listener_ = &listener;
    return ESP_OK;
}

void EspNowOtaTrigger::disarm()
{
    listener_ = nullptr;
}

void EspNowOtaTrigger::notify()
{
    if (listener_) {
        ESP_LOGI(TAG, "Notifying listener of ESP-NOW OTA trigger");
        listener_->on_ota_triggered(OtaTriggerSource::ESPNOW);
    }
}
