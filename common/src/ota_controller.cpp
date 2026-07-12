// common/src/ota_controller.cpp

#include "ota_controller.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "OtaController";

OtaController::OtaController(
    wifi_manager::IWiFiManager& wifi,
    IOtaManager& ota,
    idf_hals::IHalFreertos& rtos,
    const OtaControllerConfig& config)
    : wifi_(wifi)
    , ota_(ota)
    , rtos_(rtos)
    , config_(config)
{
}

OtaController::~OtaController()
{
    stop();
}

esp_err_t OtaController::start(std::vector<IOtaTrigger*> triggers)
{
    if (task_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    triggers_ = std::move(triggers);
    for (auto* trigger : triggers_) {
        if (trigger) {
            trigger->arm(*this);
        }
    }

    BaseType_t res = rtos_.task_create(
        &OtaController::task_fn,
        "ota_ctrl",
        config_.task_stack_size,
        this,
        config_.task_priority,
        &task_
    );
    return res == pdPASS ? ESP_OK : ESP_FAIL;
}

void OtaController::stop()
{
    for (auto* trigger : triggers_) {
        if (trigger) {
            trigger->disarm();
        }
    }
    triggers_.clear();

    if (task_ != nullptr) {
        rtos_.task_delete(task_);
        task_ = nullptr;
    }
    state_ = State::IDLE;
    connected_by_us_ = false;
}

bool OtaController::is_busy() const
{
    return state_ != State::IDLE;
}

void OtaController::on_ota_triggered(OtaTriggerSource source)
{
    ESP_LOGI(TAG, "OTA triggered from source: %d", static_cast<int>(source));
    if (task_ != nullptr) {
        rtos_.task_notify(task_, 0, eNoAction);
    }
}

void OtaController::task_fn(void* arg)
{
    auto* self = static_cast<OtaController*>(arg);
    while (true) {
        uint32_t val = 0;
        self->rtos_.task_notify_wait(0, 0, &val, portMAX_DELAY);

        if (self->state_ == State::IDLE) {
            self->state_ = State::WIFI_CONNECTING;
            while (self->state_ != State::IDLE) {
                self->run_fsm();
            }
        }
    }
}

void OtaController::run_fsm()
{
    switch (state_) {
    case State::IDLE:
        break;

    case State::WIFI_CONNECTING: {
        ESP_LOGI(TAG, "FSM: Connecting WiFi...");
        wifi_manager::State wifi_state = wifi_.get_state();
        if (wifi_state == wifi_manager::State::CONNECTED_GOT_IP) {
            ESP_LOGI(TAG, "WiFi already connected.");
            connected_by_us_ = false;
            state_ = State::OTA_RUNNING;
        }
        else {
            ESP_LOGI(TAG, "Starting WiFi connection...");
            esp_err_t err = wifi_.connect(config_.wifi_connect_timeout_ms);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "WiFi connected successfully.");
                connected_by_us_ = true;
                state_ = State::OTA_RUNNING;
            }
            else {
                ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(err));
                state_ = State::OTA_FAILED;
            }
        }
        break;
    }

    case State::OTA_RUNNING: {
        ESP_LOGI(TAG, "FSM: Starting OTA...");
        if (!ota_.start_ota()) {
            ESP_LOGE(TAG, "OTA Manager start rejected.");
            state_ = State::OTA_FAILED;
            break;
        }

        ESP_LOGI(TAG, "OTA in progress. Waiting for status change...");
        int64_t start_ms = esp_timer_get_time() / 1000;
        int64_t timeout_ms = config_.ota_watchdog_timeout_ms;

        while (true) {
            OtaStatus status = ota_.get_status();
            if (status == OtaStatus::READY_TO_RESTART) {
                ESP_LOGI(TAG, "OTA successfully completed! Waiting for reboot.");
                state_ = State::IDLE;
                break;
            }
            else if (status == OtaStatus::FAILED) {
                ESP_LOGE(TAG, "OTA failed status received.");
                state_ = State::OTA_FAILED;
                break;
            }

            int64_t elapsed_ms = (esp_timer_get_time() / 1000) - start_ms;
            if (elapsed_ms > timeout_ms) {
                ESP_LOGE(TAG, "OTA overall watchdog timeout reached. Cancelling OTA.");
                ota_.cancel_ota();
                state_ = State::OTA_FAILED;
                break;
            }

            rtos_.task_delay(pdMS_TO_TICKS(1000));
        }
        break;
    }

    case State::OTA_FAILED: {
        ESP_LOGI(TAG, "FSM: OTA Failed. Performing cleanup...");
        if (connected_by_us_) {
            ESP_LOGI(TAG, "Disconnecting WiFi session established by us...");
            wifi_.disconnect();
            connected_by_us_ = false;
        }
        state_ = State::IDLE;
        break;
    }
    }
}
