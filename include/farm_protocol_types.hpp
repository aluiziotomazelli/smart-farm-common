#pragma once

#include <cstddef>
#include <cstdint>

namespace farm {

/**
 * @brief Application-specific Node IDs for the Smart Farm ecosystem.
 */
enum class NodeId : uint8_t
{
    UNKNOWN = 0x00,      ///< Uninitialized or unassigned Node ID
    HUB = 0x01,          ///< Reserved: Central Gateway/Hub controller
    WATER_TANK = 0x05,   ///< Water tank monitoring peripheral node
    SOLAR_SENSOR = 0x07, ///< Solar power sensor peripheral node
    PUMP_CONTROL = 0x0A, ///< Water pump control actuator node
    WEATHER = 0x0C,      ///< Weather station sensor node
    BROADCAST = 0xFF,    ///< Reserved: Broadcast destination to all nodes
};

/**
 * @brief Application-specific Node Types for the Smart Farm ecosystem.
 */
enum class NodeType : uint8_t
{
    UNKNOWN = 0x00,  ///< Unspecified or uninitialized node type
    HUB = 0x01,      ///< Central Gateway/Hub controller
    SENSOR = 0x02,   ///< Telemetry and sensor data gathering node
    ACTUATOR = 0x03, ///< Control and output activation node
};

/**
 * @brief Application-specific Data Payload Types (MessageType::DATA).
 * @note Range: 0x01–0x3F for telemetry reports; 0x40+ for application events.
 */
enum class PayloadType : uint8_t
{
    WATER_LEVEL_REPORT = 0x01,  ///< Water tank level, battery, and sensor telemetry
    SOLAR_SENSOR_REPORT = 0x02, ///< Solar voltage, current, and power telemetry
    WEATHER_REPORT = 0x03,      ///< Weather sensor data telemetry
    PUMP_CONTROL_STATUS = 0x04, ///< Pump operation status and circuit state
    OTA_STATUS_REPORT = 0x45,   ///< Over-The-Air firmware update outcome report
    REQUEST_SYNC_TIME = 0x46,   ///< Request for time synchronization from the Hub
};

/**
 * @brief Application-specific Command Types (MessageType::COMMAND).
 * @note Range: 0x40–0xFF (0x01–0x3F is reserved for generic transport commands in espnow::CommandType).
 */
enum class CommandType : uint8_t
{
    SLEEP_OVERRIDE = 0x40, ///< Instructs a sleeping node to override its local sleep duration
    PUMP_TURN_ON = 0x41,   ///< Instructs the pump actuator to activate
    PUMP_TURN_OFF = 0x42,  ///< Instructs the pump actuator to deactivate
    SYNC_TIME = 0x43,      ///< Instructs a node to synchronize system time via ESP-NOW
};

/**
 * @brief Measurement status mirrored from hardware sensor drivers.
 */
enum class SensorStatus : uint8_t
{
    OK = 0x00,                 ///< Measurement valid and nominal
    WARNING_LOW_SIGNAL = 0x01, ///< Low signal quality or weak echo detected
    ERROR_TIMEOUT = 0x02,      ///< Echo/response timeout reached
    ERROR_OUT_OF_RANGE = 0x03, ///< Distance/measurement exceeds physical sensor bounds
    ERROR_UNSTABLE = 0x04,     ///< High variance or jitter between consecutive samples
    ERROR_HARDWARE = 0x05,     ///< Driver or power rail hardware failure
    UNKNOWN = 0xFF,            ///< Uninitialized sensor status
};

/**
 * @brief Represents battery health and operational state based on voltage thresholds.
 */
enum class BatteryState : uint8_t
{
    UNKNOWN = 0x00,  ///< Uninitialized or unmeasured battery state
    CRITICAL = 0x01, ///< Voltage below safe operation limit; immediate sleep required
    LOW = 0x02,      ///< Voltage low; non-essential tasks should be reduced
    NORMAL = 0x03,   ///< Battery operating within optimal voltage range
    FULL = 0x04,     ///< Battery fully charged or powered via external source
};

/**
 * @brief Defines who controls the load's switching behavior.
 *
 * Reported by the actuator node in its status payload. In AUTO mode, the
 * actuator requests hub authorization before switching. In MANUAL mode,
 * the actuator acts on the user's physical switch and reports its state.
 */
enum class ControlMode : uint8_t
{
    OFF = 0x00,    ///< Load is physically off, regardless of source selection
    AUTO = 0x01,   ///< Hub-managed: actuator requests authorization before switching
    MANUAL = 0x02, ///< User-controlled: actuator acts independently, reports state to hub
};

/**
 * @brief Defines the energy source circuit a load is connected to.
 *
 * Only meaningful when ControlMode != OFF. SOLAR loads are counted in the
 * hub's power balance. GRID loads are monitored for display/log only.
 */
enum class PowerSource : uint8_t
{
    UNKNOWN = 0x00, ///< Source not yet determined (initial state or actuator not paired)
    SOLAR = 0x01,   ///< Connected to solar/inverter circuit
    GRID = 0x02,    ///< Connected to utility grid circuit (fallback)
};

/**
 * @brief Execution outcome of an Over-The-Air (OTA) firmware update operation.
 */
enum class OtaExecResult : uint8_t
{
    DOWNLOAD_FAILED = 0,   ///< Failed to download manifest/image or SHA256 hash mismatch
    CONFIRMED_SUCCESS = 1, ///< New firmware executed cleanly and passed post-boot verification
    ROLLBACK_TRIGGERED = 2 ///< New firmware failed post-boot health check; rolled back to previous image
};

/**
 * @brief Detailed error reason for OTA update failures across all farm nodes.
 */
enum class OtaErrorCode : uint8_t
{
    NONE = 0x00,                     ///< No error / success
    MANIFEST_PARSE_ERROR = 0x01,     ///< Failed to parse JSON manifest or invalid structure
    VERSION_NOT_NEWER = 0x02,        ///< Version in manifest is older or equal to running image
    WIFI_CONNECT_FAILED = 0x03,      ///< Failed to connect to WiFi AP for download
    HTTP_DOWNLOAD_FAILED = 0x04,     ///< HTTP request failed or connection timed out
    IMAGE_HASH_MISMATCH = 0x05,      ///< SHA-256 validation failed for downloaded image
    FLASH_WRITE_ERROR = 0x06,        ///< Failed to write binary to flash partition
    HEALTH_CHECK_FAILED = 0x07,      ///< Post-boot health check failed (unhealthy session)
    PARTITION_CONFIRM_FAILED = 0x08, ///< esp_ota_mark_app_valid() returned failure
    WATCHDOG_TIMEOUT = 0x09,         ///< OTA process exceeded overall watchdog timeout
    DEVICE_TYPE_MISMATCH = 0x0A,     ///< Manifest device_type does not match node configuration
    DOWNLOAD_SESSION_FAIL = 0x0B,    ///< Failed to initialize OTA download session or image descriptor
    UNKNOWN_ERROR = 0xFF,            ///< Unclassified or unexpected failure
};

#pragma pack(push, 1)

/**
 * @brief Telemetry report sent periodically by the Water Tank node.
 */
struct WaterLevelReport
{
    uint16_t level_permille;    ///< Calculated level in permille (0 to 1000)
    float distance_cm;          ///< Raw measured distance to water surface in centimeters
    uint16_t battery_mv;        ///< Battery voltage in millivolts
    uint8_t battery_percent;    ///< Calculated battery percentage (0 to 100)
    BatteryState battery_state; ///< Current battery state classification
    SensorStatus status;        ///< Distance sensor measurement status
    bool float_switch_is_full;  ///< True if mechanical float switch indicates full tank
    bool backup_mode_active;    ///< True if running in fail-safe/backup estimation mode
    uint64_t unix_time;         ///< Epoch UTC timestamp in ms at sample time (0 if not synchronized)
};

/**
 * @brief Telemetry report sent by solar monitoring nodes.
 */
struct SolarSensorReport
{
    uint16_t voltage_mv; ///< Panel voltage in millivolts
    uint16_t current_ma; ///< Generated current in milliamperes
    uint16_t power_mw;   ///< Calculated instantaneous power in milliwatts
};

/**
 * @brief Command payload sent to override a node's deep sleep interval.
 */
struct SleepOverrideCommand
{
    uint32_t sleep_time_s; ///< Requested sleep duration in seconds (0 = cancel override)
};

/**
 * @brief Command payload sent to control pump activation and safety parameters.
 */
struct PumpCommand
{
    CommandType action;  ///< Command action (PUMP_TURN_ON or PUMP_TURN_OFF)
    uint16_t watchdog_s; ///< Auto-off watchdog timeout in seconds (0 = disable watchdog)
    uint8_t circuit_id;  ///< Target pump/circuit ID (0 = default primary circuit)
};

/**
 * @brief Command payload sent to synchronize system time node-to-node via ESP-NOW.
 * Layout is identical to time_manager::TimeSyncPacket (12 bytes packed).
 */
struct TimeSyncCommand
{
    uint64_t timestamp_ms; ///< Epoch UTC timestamp in milliseconds
    int16_t tz_offset_min; ///< Timezone offset in minutes (e.g. -240 for UTC-4)
    uint8_t sync_source;   ///< Synchronization source (0=unknown, 1=SNTP, 2=manual, 3=ESP-NOW)
    uint8_t flags;         ///< Bit 0: is_valid (1 if synchronized)
};

/**
 * @brief Status report notifying the Hub about an OTA update execution result.
 */
struct OtaStatusReport
{
    OtaExecResult result;    ///< Execution result of the OTA process
    OtaErrorCode error_code; ///< Detailed error reason (NONE if success)
    uint8_t fw_major;        ///< Running firmware major version number
    uint8_t fw_minor;        ///< Running firmware minor version number
    uint8_t fw_patch;        ///< Running firmware patch version number
};

#pragma pack(pop)

} // namespace farm

// Validations to ensure no payload exceeds maximum ESP-NOW payload bounds
static constexpr size_t APP_MAX_PAYLOAD_SIZE = 230;
static_assert(
    sizeof(farm::WaterLevelReport) <= APP_MAX_PAYLOAD_SIZE,
    "WaterLevelReport payload exceeds ESP-NOW payload limit");
static_assert(
    sizeof(farm::SolarSensorReport) <= APP_MAX_PAYLOAD_SIZE,
    "SolarSensorReport payload exceeds ESP-NOW payload limit");
static_assert(
    sizeof(farm::SleepOverrideCommand) <= APP_MAX_PAYLOAD_SIZE,
    "SleepOverrideCommand payload exceeds ESP-NOW payload limit");
static_assert(
    sizeof(farm::TimeSyncCommand) <= APP_MAX_PAYLOAD_SIZE,
    "TimeSyncCommand payload exceeds ESP-NOW payload limit");
static_assert(sizeof(farm::PumpCommand) <= APP_MAX_PAYLOAD_SIZE, "PumpCommand payload exceeds ESP-NOW payload limit");
static_assert(
    sizeof(farm::OtaStatusReport) <= APP_MAX_PAYLOAD_SIZE,
    "OtaStatusReport payload exceeds ESP-NOW payload limit");
