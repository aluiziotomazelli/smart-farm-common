// common/include/espnow_ota_trigger.hpp
#pragma once

#include "interfaces/i_ota_trigger.hpp"

class EspNowOtaTrigger : public IOtaTrigger
{
public:
    esp_err_t arm(IOtaTriggerListener& listener) override;
    void disarm() override;

    void notify();

private:
    IOtaTriggerListener* listener_ = nullptr;
};
