#pragma once

#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "interfaces/i_ota_trigger.hpp"
#include "interfaces/i_wifi_manager.hpp"
#include "interfaces/i_ota_manager.hpp"
#include "interfaces/i_hal_freertos.hpp"

#include "esp_err.h"

/**
 * @struct OtaControllerConfig
 * @brief Configuration settings for the OTA Controller.
 */
struct OtaControllerConfig
{
    uint32_t wifi_connect_timeout_ms = 15000;   ///< Max time to wait for WiFi connection (ms)
    uint32_t ota_watchdog_timeout_ms = 120000;  ///< Overall watchdog timeout for OTA process (ms)
    uint32_t task_stack_size = 4096;            ///< Stack size for the internal FSM task
    uint8_t task_priority = 5;                  ///< Priority for the internal FSM task
};

/**
 * @class OtaController
 * @brief Orchestrator FSM for managing WiFi connection and OTA firmware update processes.
 *
 * Implements the IOtaTriggerListener interface to receive trigger events from different sources.
 */
class OtaController : public IOtaTriggerListener
{
public:
    /**
     * @brief State of the OTA Controller FSM.
     */
    enum class State
    {
        IDLE,             ///< Controller is idle, waiting for a trigger event
        WIFI_CONNECTING,  ///< WiFi connection is being established
        OTA_RUNNING,      ///< OTA update is running in background via OtaManager
        OTA_FAILED        ///< OTA run failed, performing cleanup
    };

    /**
     * @brief Construct a new Ota Controller object.
     *
     * @param wifi Reference to the WiFi Manager.
     * @param ota Reference to the OTA Manager.
     * @param rtos Reference to the FreeRTOS HAL abstraction.
     * @param config Optional configuration settings.
     */
    OtaController(
        wifi_manager::IWiFiManager& wifi,
        IOtaManager& ota,
        idf_hals::IHalFreertos& rtos,
        const OtaControllerConfig& config = {});

    /**
     * @brief Destroy the Ota Controller object (stops the running task).
     */
    ~OtaController() override;

    /**
     * @brief Start the OTA controller worker task and register triggers.
     *
     * @param triggers Vector of triggers to arm.
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if already started/running
     * @return ESP_FAIL if task creation fails
     */
    esp_err_t start(std::vector<IOtaTrigger*> triggers);

    /**
     * @brief Stop the OTA controller worker task and disarm all triggers.
     */
    void stop();

    /**
     * @brief Check if the controller is currently busy executing an OTA sequence.
     *
     * @return true if FSM state is not IDLE, false otherwise.
     */
    bool is_busy() const;

    /** @copydoc IOtaTriggerListener::on_ota_triggered */
    void on_ota_triggered(OtaTriggerSource source) override;

private:
    static constexpr uint32_t NOTIFY_OTA_TRIGGER    = (1 << 0); ///< Task notification bit for triggering OTA
    static constexpr uint32_t NOTIFY_TASK_TO_STOP   = (1 << 1); ///< Task notification bit for stopping task
    static constexpr uint32_t NOTIFY_ALL            = NOTIFY_OTA_TRIGGER | NOTIFY_TASK_TO_STOP;
    static constexpr uint32_t kOtaCancelWatchdogMs  = 10000;    ///< Watchdog applied after cancel_ota() (ms)

    friend class OtaControllerTest;

    /**
     * @brief Worker task entry point.
     *
     * @param arg Pointer to OtaController instance.
     */
    static void task_fn(void* arg);

    /**
     * @brief Calculate the maximum safety wait timeout for stop().
     *
     * @return Maximum wait time in milliseconds.
     */
    uint32_t compute_stop_timeout_ms() const;

    /**
     * @brief Core FSM state transition and execution logic.
     */
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
