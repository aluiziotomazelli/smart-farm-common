#pragma once

#include "farm_protocol_types.hpp"
#include <stdbool.h>
#include <stdint.h>

// =============================
//  Power profile
// =============================
enum class PowerProfile : uint8_t
{
    ALWAYS_ON = 0,
    LOW_POWER,
    DEEP_SLEEP,
};

// =============================
//  Wakeup source
// =============================
enum class WakeSource : uint8_t
{
    NONE = 0,
    TIMER,
    GPIO,
    POWER_ON,
};

// Flat struct for simplicity
struct CoreStorage
{
    // Schema identifier, not used in comparator checks
    static constexpr uint32_t CORE_MAGIC = 0x434F5245; // "CORE"
    static constexpr uint32_t CORE_VERSION = 1;

    // Magic & Schema
    uint32_t magic = CORE_MAGIC;
    uint32_t version = CORE_VERSION;

    // Identity (inline, no separate struct)
    farm::NodeId node_id = farm::NodeId::UNKNOWN;
    farm::NodeType node_type = farm::NodeType::UNKNOWN;
    uint8_t hw_revision = 1;

    // Firmware
    uint8_t fw_major = 0;
    uint8_t fw_minor = 1;
    uint8_t fw_patch = 0;

    // Lifecycle
    uint32_t boot_count = 0;
    uint32_t crash_count = 0;

    // Time
    bool has_valid_time = false;
    uint64_t unix_time = 0;
    uint32_t last_sync_uptime = 0;

    // Power
    PowerProfile power_profile = PowerProfile::ALWAYS_ON;
    uint32_t sleep_interval_s = 3600;

    // Wake
    WakeSource last_wake = WakeSource::POWER_ON;

    // CRC validation (MUST BE LAST FIELD)
    uint32_t crc = 0;

    void reset()
    {
        *this = {};
        magic = CORE_MAGIC;
        version = CORE_VERSION;
    }

    bool operator==(const CoreStorage& other) const
    {
        return magic == other.magic && version == other.version && node_id == other.node_id &&
               node_type == other.node_type && hw_revision == other.hw_revision && fw_major == other.fw_major &&
               fw_minor == other.fw_minor && fw_patch == other.fw_patch && boot_count == other.boot_count &&
               crash_count == other.crash_count && has_valid_time == other.has_valid_time &&
               unix_time == other.unix_time && last_sync_uptime == other.last_sync_uptime &&
               power_profile == other.power_profile && sleep_interval_s == other.sleep_interval_s &&
               last_wake == other.last_wake;
    }

    bool operator!=(const CoreStorage& other) const { return !(*this == other); }
};