#pragma once

#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "interfaces/i_ota_trigger.hpp"
#include "interfaces/i_wifi_manager.hpp"
#include "interfaces/i_ota_manager.hpp"
#include "interfaces/i_hal_freertos.hpp"

#include "esp_err.h"

struct OtaControllerConfig
{
    uint32_t wifi_connect_timeout_ms = 15000;
    uint32_t ota_watchdog_timeout_ms = 120000;
    uint32_t task_stack_size = 4096;
    uint8_t task_priority = 5;
};

class OtaController : public IOtaTriggerListener
{
public:
    enum class State
    {
        IDLE,
        WIFI_CONNECTING,
        OTA_RUNNING,
        OTA_FAILED
    };

    OtaController(
        wifi_manager::IWiFiManager& wifi,
        IOtaManager& ota,
        idf_hals::IHalFreertos& rtos,
        const OtaControllerConfig& config = {});

    ~OtaController() override;

    esp_err_t start(std::vector<IOtaTrigger*> triggers);
    void stop();

    bool is_busy() const;

    // IOtaTriggerListener
    void on_ota_triggered(OtaTriggerSource source) override;

private:
    static constexpr uint32_t NOTIFY_OTA_TRIGGER    = (1 << 0);
    static constexpr uint32_t NOTIFY_TASK_TO_STOP   = (1 << 1);
    static constexpr uint32_t NOTIFY_ALL            = NOTIFY_OTA_TRIGGER | NOTIFY_TASK_TO_STOP;
    // Watchdog applied after cancel_ota() — must be < ota_watchdog_timeout_ms.
    static constexpr uint32_t kOtaCancelWatchdogMs  = 10000;

    friend class OtaControllerTest;

    static void task_fn(void* arg);
    uint32_t compute_stop_timeout_ms() const;
    void run_fsm();

    wifi_manager::IWiFiManager& wifi_;
    IOtaManager& ota_;
    idf_hals::IHalFreertos& rtos_;
    OtaControllerConfig config_;
    std::vector<IOtaTrigger*> triggers_;

    State state_ = State::IDLE;
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t task_done_semaphore_ = nullptr;
    bool stop_requested_ = false;
    bool connected_by_us_ = false;
};
