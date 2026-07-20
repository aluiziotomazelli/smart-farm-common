#pragma once

#include <cstddef>
#include <cstdint>

namespace farm {

/**
 * @brief Application-specific Node IDs for the Farm project.
 */
enum class NodeId : uint8_t
{
    UNKNOWN      = 0x00,
    HUB          = 0x01, // Reserved: Central Hub
    WATER_TANK   = 0x05,
    SOLAR_SENSOR = 0x07,
    PUMP_CONTROL = 0x0A,
    WEATHER      = 0x0C,
    BROADCAST    = 0xFF, // Reserved: Broadcast
};

/**
 * @brief Application-specific Node Types for the Farm project.
 */
enum class NodeType : uint8_t
{
    UNKNOWN  = 0x00,
    HUB      = 0x01, // Reserved: Central Hub
    SENSOR   = 0x02,
    ACTUATOR = 0x03,
};

/**
 * @brief Application-specific Data Payload Types (MessageType::DATA) for the Farm project.
 * Range: 0x01–0x3F
 */
enum class PayloadType : uint8_t
{
    WATER_LEVEL_REPORT  = 0x01,
    SOLAR_SENSOR_REPORT = 0x02,
    WEATHER_REPORT      = 0x03,
    PUMP_CONTROL_STATUS = 0x04,
};

/**
 * @brief Application-specific Command Types (MessageType::COMMAND) for the Farm project.
 * Range: 0x40–0xFF (0x01–0x3F is reserved for generic transport commands in espnow::CommandType).
 */
enum class CommandType : uint8_t
{
    SLEEP_OVERRIDE = 0x40, ///< Instructs a sleeping node to override local sleep calculation
    PUMP_TURN_ON   = 0x41, ///< Instructs the pump actuator to activate
    PUMP_TURN_OFF  = 0x42, ///< Instructs the pump actuator to deactivate
};

/**
 * @brief Measurement status mirrored from sensor components.
 */
enum class SensorStatus : uint8_t
{
    OK                 = 0x00,
    WARNING_LOW_SIGNAL = 0x01,
    ERROR_TIMEOUT      = 0x02,
    ERROR_OUT_OF_RANGE = 0x03,
    ERROR_UNSTABLE     = 0x04,
    ERROR_HARDWARE     = 0x05,
    UNKNOWN            = 0xFF,
};

/**
 * @brief Represents the state of the battery based on voltage thresholds.
 */
enum class BatteryState : uint8_t
{
    UNKNOWN  = 0x00,
    CRITICAL = 0x01,
    LOW      = 0x02,
    NORMAL   = 0x03,
    FULL     = 0x04,
};

#pragma pack(push, 1)

struct WaterLevelReport
{
    uint16_t     level_permille;
    float        distance_cm;
    uint16_t     battery_mv;
    uint8_t      battery_percent;
    BatteryState battery_state;
    SensorStatus status;
    bool         float_switch_is_full;
    bool         backup_mode_active;
};

struct SolarSensorReport
{
    uint16_t voltage_mv;
    uint16_t current_ma;
    uint16_t power_mw;
};

struct SleepOverrideCommand
{
    uint32_t sleep_time_s; ///< Requested sleep duration in seconds (0 = cancel override)
};

struct PumpCommand
{
    CommandType action;     ///< PUMP_TURN_ON or PUMP_TURN_OFF
    uint16_t    watchdog_s; ///< Auto-off if hub is silent for this duration (safety)
    uint8_t     circuit_id; ///< Which pump/circuit (0 = default, for future expansion)
};

#pragma pack(pop)

} // namespace farm

// Validations to ensure that no payload exceeds the ESP-NOW payload limits
static constexpr size_t APP_MAX_PAYLOAD_SIZE = 230;
static_assert(sizeof(farm::WaterLevelReport) <= APP_MAX_PAYLOAD_SIZE, "WaterLevelReport payload is too large");
static_assert(sizeof(farm::SolarSensorReport) <= APP_MAX_PAYLOAD_SIZE, "SolarSensorReport payload is too large");
static_assert(sizeof(farm::SleepOverrideCommand) <= APP_MAX_PAYLOAD_SIZE, "SleepOverrideCommand payload is too large");
static_assert(sizeof(farm::PumpCommand) <= APP_MAX_PAYLOAD_SIZE, "PumpCommand payload is too large");
