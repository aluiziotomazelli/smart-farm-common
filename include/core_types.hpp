#pragma once

#include <cstdint>

#include "app_storage.hpp"
#include "farm_protocol_types.hpp"

// =============================
//  Wakeup source
// =============================
enum class WakeSource : uint8_t
{
    UNKNOWN = 0,
    TIMER,
    GPIO,
    POWER_ON,
    RESTART,
    CRASH,
};

// =============================
//  Core Storage Constants
// =============================
static constexpr uint32_t CORE_MAGIC = 0x434F5245; // "CORE"
static constexpr uint8_t CORE_VERSION = 1;

/**
 * @struct CoreData
 * @brief Pure domain struct representing common node state and identity.
 *
 * Contains no storage metadata (magic, version, crc) which are managed by the persistence layer.
 */
struct CoreData
{
    // Identity
    farm::NodeId node_id = farm::NodeId::UNKNOWN;
    farm::NodeType node_type = farm::NodeType::UNKNOWN;
    uint8_t hw_revision = 1;

    // Firmware
    uint8_t fw_major = 0;
    uint8_t fw_minor = 0;
    uint8_t fw_patch = 0;

    // Lifecycle
    uint32_t boot_count = 0;
    uint32_t crash_count = 0;

    // Time
    bool has_valid_time = false;
    uint64_t last_sync_unix_time_ms = 0;

    // Power
    farm::PowerProfile power_profile = farm::PowerProfile::ALWAYS_ON;
    uint32_t sleep_interval_s = 3600;

    // Wake
    WakeSource last_wake = WakeSource::UNKNOWN;

    void reset() { *this = {}; }

    bool operator==(const CoreData& other) const
    {
        return node_id == other.node_id && node_type == other.node_type && hw_revision == other.hw_revision &&
               fw_major == other.fw_major && fw_minor == other.fw_minor && fw_patch == other.fw_patch &&
               boot_count == other.boot_count && crash_count == other.crash_count &&
               has_valid_time == other.has_valid_time &&
               last_sync_unix_time_ms == other.last_sync_unix_time_ms &&
               power_profile == other.power_profile && sleep_interval_s == other.sleep_interval_s &&
               last_wake == other.last_wake;
    }

    bool operator!=(const CoreData& other) const { return !(*this == other); }
};

/**
 * @brief CoreStorage envelope alias used for allocating physical RTC/NVS storage buffers.
 */
using CoreStorage = StorageEnvelope<CoreData, CORE_MAGIC, CORE_VERSION>;