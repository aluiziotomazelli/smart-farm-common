// common/src/ota_controller.cpp

#include "ota_controller.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include <algorithm>
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

    task_done_semaphore_ = rtos_.semaphore_create_binary();
    if (task_done_semaphore_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    stop_requested_ = false;
    triggers_ = std::move(triggers);
    for (auto* trigger : triggers_) {
        if (trigger) {
            trigger->arm(*this);
        }
    }

    BaseType_t res = rtos_.task_create(
        &OtaController::task_fn, "ota_ctrl", config_.task_stack_size, this, config_.task_priority, &task_);
    if (res != pdPASS) {
        rtos_.semaphore_delete(task_done_semaphore_);
        task_done_semaphore_ = nullptr;
        return ESP_FAIL;
    }
    return ESP_OK;
}

uint32_t OtaController::compute_stop_timeout_ms() const
{
    constexpr uint32_t SAFETY_MARGIN_MS = 2000;
    return std::max(config_.wifi_connect_timeout_ms, OTA_CANCEL_WATCHDOG_MS) + SAFETY_MARGIN_MS;
}

void OtaController::stop()
{
    for (auto* trigger : triggers_) {
        if (trigger) {
            trigger->disarm();
        }
    }
    triggers_.clear();

    TaskHandle_t task_to_stop = task_;

    if (task_to_stop != nullptr) {
        ESP_LOGI(TAG, "Requesting OTA task to stop gracefully...");
        rtos_.task_notify(task_to_stop, NOTIFY_TASK_TO_STOP, eSetBits);

        const uint32_t stop_timeout_ms = compute_stop_timeout_ms();
        bool exited_cleanly = rtos_.semaphore_take(task_done_semaphore_, pdMS_TO_TICKS(stop_timeout_ms)) == pdPASS;

        if (!exited_cleanly) {
            ESP_LOGW(TAG, "Task stop timed out after %lums. Forcing deletion.", (unsigned long)stop_timeout_ms);
            rtos_.task_delete(task_to_stop);
        }
    }

    if (task_done_semaphore_ != nullptr) {
        rtos_.semaphore_delete(task_done_semaphore_);
        task_done_semaphore_ = nullptr;
    }

    task_ = nullptr;
    state_ = State::IDLE;
    connected_by_us_ = false;
    stop_requested_ = false;
}

bool OtaController::is_busy() const
{
    return state_ != State::IDLE;
}

void OtaController::on_ota_triggered(OtaTriggerSource source)
{
    if (is_busy()) {
        ESP_LOGW(TAG, "OTA trigger ignored, controller is busy (state=%d)", static_cast<int>(state_));
        return;
    }

    ESP_LOGI(TAG, "OTA triggered from source: %d", static_cast<int>(source));
    if (task_ != nullptr) {
        rtos_.task_notify(task_, NOTIFY_OTA_TRIGGER, eSetBits);
    }
}

void OtaController::task_fn(void* arg)
{
    auto* self = static_cast<OtaController*>(arg);
    bool should_stop = false;

    while (!should_stop) {
        uint32_t notifications = 0;
        TickType_t wait_ticks = (self->state_ == State::IDLE) ? idf_hals::PORT_MAX_DELAY : 0;

        if (self->rtos_.task_notify_wait(0, NOTIFY_ALL, &notifications, wait_ticks) == pdTRUE) {
            if (notifications & NOTIFY_TASK_TO_STOP) {
                self->stop_requested_ = true;
            }
            if ((notifications & NOTIFY_OTA_TRIGGER) && self->state_ == State::IDLE) {
                self->state_ = State::WIFI_CONNECTING;
            }
        }

        if (self->stop_requested_ && self->state_ == State::IDLE) {
            should_stop = true;
            break;
        }

        if (self->state_ != State::IDLE) {
            self->run_fsm();
        }
    }

    ESP_LOGI(TAG, "OTA task exiting cleanly.");
    self->rtos_.semaphore_give(self->task_done_semaphore_);
    self->rtos_.task_delete(nullptr);
}

void OtaController::run_fsm()
{
    switch (state_) {
    case State::IDLE:
        break;

    case State::WIFI_CONNECTING:
    {
        ESP_LOGI(TAG, "FSM: Connecting WiFi...");
        wifi_manager::State wifi_state = wifi_.get_state();
        if (wifi_state == wifi_manager::State::CONNECTED_GOT_IP) {
            ESP_LOGI(TAG, "WiFi already connected.");
            connected_by_us_ = false;
            state_ = State::OTA_RUNNING;
        }
        else {
            ESP_LOGI(TAG, "Starting WiFi connection...");
            // Note: wifi_.connect() is idempotent. If WiFi is already CONNECTING,
            // CONNECTED_NO_IP, or CONNECTED_GOT_IP, the WiFiManager's validate_command
            // returns Action::SKIP and connect() immediately returns ESP_OK.
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

    case State::OTA_RUNNING:
    {
        ESP_LOGI(TAG, "FSM: Starting OTA...");
        if (!ota_.start_ota()) {
            ESP_LOGE(TAG, "OTA Manager start rejected.");
            state_ = State::OTA_FAILED;
            break;
        }

        ESP_LOGI(TAG, "OTA in progress. Waiting for status change...");
        int64_t start_ms = esp_timer_get_time() / 1000;
        int64_t timeout_ms = config_.ota_watchdog_timeout_ms;

        bool cancel_requested = false;

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

            uint32_t pending = 0;
            rtos_.task_notify_wait(0, NOTIFY_ALL, &pending, 0);
            if ((pending & NOTIFY_TASK_TO_STOP) && !cancel_requested) {
                ESP_LOGW(TAG, "Stop requested mid-OTA. Cancelling cooperatively...");
                ota_.cancel_ota();
                cancel_requested = true;
                start_ms = esp_timer_get_time() / 1000;
                timeout_ms = OTA_CANCEL_WATCHDOG_MS;
            }

            int64_t elapsed_ms = (esp_timer_get_time() / 1000) - start_ms;
            if (elapsed_ms > timeout_ms) {
                ESP_LOGE(
                    TAG,
                    "%s",
                    cancel_requested ? "OTA cancel watchdog expired."
                                     : "OTA overall watchdog timeout reached. Cancelling OTA.");
                ota_.cancel_ota();
                state_ = State::OTA_FAILED;
                break;
            }

            rtos_.task_delay(pdMS_TO_TICKS(1000));
        }
        break;
    }

    case State::OTA_FAILED:
    {
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
