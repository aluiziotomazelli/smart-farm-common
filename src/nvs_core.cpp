#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "nvs_core.hpp"

static const char* TAG = "NvsCore";

NvsCore::NvsCore(IPersistenceBackend& rtc_core, IPersistenceBackend& nvs_core)
    : AppStorage<CoreData, CORE_MAGIC, CORE_VERSION>(rtc_core, nvs_core, "NvsCore")
{
}

void NvsCore::process_boot_reasons(
    CoreData& core,
    esp_reset_reason_t reset_reason,
    esp_sleep_wakeup_cause_t wakeup_cause,
    bool& out_pending_commit)
{
    core.boot_count++;

    switch (reset_reason) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
        core.crash_count++;
        out_pending_commit = true;
        core.last_wake = WakeSource::CRASH;
        ESP_LOGW(TAG, "Reset from crash");
        break;

    case ESP_RST_POWERON:
        core.last_wake = WakeSource::POWER_ON;
        break;

    case ESP_RST_SW:
        core.last_wake = WakeSource::RESTART;
        break;

    case ESP_RST_DEEPSLEEP:
        switch (wakeup_cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            core.last_wake = WakeSource::TIMER;
            break;

        case ESP_SLEEP_WAKEUP_EXT0:
        case ESP_SLEEP_WAKEUP_EXT1:
        case ESP_SLEEP_WAKEUP_GPIO:
            core.last_wake = WakeSource::GPIO;
            break;

        default:
            core.last_wake = WakeSource::UNKNOWN;
            break;
        }
        break;

    default:
        core.last_wake = WakeSource::UNKNOWN;
        break;
    }
}
