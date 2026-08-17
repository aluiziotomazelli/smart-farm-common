#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esp_err.h"
#include "esp_rom_crc.h"
#include "esp_sleep.h"
#include "esp_system.h"

#include "core_types.hpp"
#include "mock_persistence_backend.hpp"
#include "nvs_core.hpp"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/**
 * @brief Helper to calculate CRC for envelope in tests.
 */
template <typename T> inline uint32_t test_calculate_crc(const T& envelope)
{
    static_assert(std::is_standard_layout_v<T>, "T must be standard_layout for safe offset calculation");
    static_assert(offsetof(T, crc) != 0, "T must have a non-first crc field");

    return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&envelope), offsetof(T, crc));
}

/**
 * @brief Fixture for NvsCore tests.
 */
class NvsCoreTest : public ::testing::Test
{
protected:
    NiceMock<MockPersistenceBackend> rtc_backend_;
    NiceMock<MockPersistenceBackend> nvs_backend_;

    std::unique_ptr<NvsCore> nvs_core_;

    void SetUp() override
    {
        rtc_backend_.UseRealStorage();
        nvs_backend_.UseRealStorage();

        nvs_core_ = std::make_unique<NvsCore>(rtc_backend_, nvs_backend_);
    }

    void TearDown() override { nvs_core_.reset(); }

    CoreData CreateValidCoreData()
    {
        CoreData core;
        core.node_id = farm::NodeId::WATER_TANK;
        core.node_type = farm::NodeType::SENSOR;
        core.hw_revision = 1;
        core.fw_major = 0;
        core.fw_minor = 1;
        core.fw_patch = 0;
        core.boot_count = 42;
        core.crash_count = 2;
        core.has_valid_time = true;
        core.last_sync_unix_time_ms = 3600;
        core.power_profile = farm::PowerProfile::DEEP_SLEEP;
        core.sleep_interval_s = 300;
        core.last_wake = WakeSource::TIMER;
        return core;
    }

    CoreStorage CreateValidCoreEnvelope()
    {
        CoreStorage env;
        env.magic = CORE_MAGIC;
        env.version = CORE_VERSION;
        env.data = CreateValidCoreData();
        env.crc = test_calculate_crc(env);
        return env;
    }

    void SetRtcEnvelope(const CoreStorage& env) { rtc_backend_.save(&env, sizeof(env)); }

    void SetNvsEnvelope(const CoreStorage& env) { nvs_backend_.save(&env, sizeof(env)); }

    CoreStorage GetStoredRtcEnvelope() const
    {
        CoreStorage env{};
        memcpy(&env, rtc_backend_.GetStoredData(), sizeof(env));
        return env;
    }

    CoreStorage GetStoredNvsEnvelope() const
    {
        CoreStorage env{};
        memcpy(&env, nvs_backend_.GetStoredData(), sizeof(env));
        return env;
    }
};

// =============================================================
// Tests
// =============================================================

TEST_F(NvsCoreTest, LoadFromRtcWhenValid)
{
    // Arrange
    CoreStorage expected_env = CreateValidCoreEnvelope();
    SetRtcEnvelope(expected_env);

    EXPECT_CALL(rtc_backend_, load).Times(::testing::AtLeast(1));
    EXPECT_CALL(nvs_backend_, load).Times(0);

    // Act
    CoreData loaded{};
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.boot_count, 42);
    EXPECT_EQ(loaded.node_id, farm::NodeId::WATER_TANK);
}

TEST_F(NvsCoreTest, LoadFromNvsWhenRtcInvalid)
{
    // Arrange: Valid data only in NVS
    CoreStorage expected_env = CreateValidCoreEnvelope();
    SetNvsEnvelope(expected_env);

    // Act
    CoreData loaded{};
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.boot_count, 42);

    // Verify RTC was synced with valid NVS data
    CoreStorage rtc_synced = GetStoredRtcEnvelope();
    EXPECT_EQ(rtc_synced.magic, CORE_MAGIC);
    EXPECT_EQ(rtc_synced.data.boot_count, 42);
}

TEST_F(NvsCoreTest, LoadFailsWhenBothInvalid)
{
    // Arrange: Both backends are empty
    CoreData loaded{};
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

TEST_F(NvsCoreTest, SaveToDirtyDataNoForce)
{
    // Arrange
    CoreData to_save = CreateValidCoreData();
    to_save.boot_count = 99;

    EXPECT_CALL(rtc_backend_, save).Times(::testing::AtLeast(1));
    EXPECT_CALL(nvs_backend_, save).Times(0);

    // Act
    esp_err_t ret = nvs_core_->save_core(to_save, false);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    CoreStorage rtc_stored = GetStoredRtcEnvelope();
    EXPECT_EQ(rtc_stored.data.boot_count, 99);
}

TEST_F(NvsCoreTest, SaveToBothWhenForceNvs)
{
    // Arrange
    CoreData to_save = CreateValidCoreData();
    to_save.boot_count = 55;

    EXPECT_CALL(rtc_backend_, save).Times(::testing::AtLeast(1));
    EXPECT_CALL(nvs_backend_, save).Times(::testing::AtLeast(1));

    // Act
    esp_err_t ret = nvs_core_->save_core(to_save, true);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    CoreStorage rtc_stored = GetStoredRtcEnvelope();
    CoreStorage nvs_stored = GetStoredNvsEnvelope();

    EXPECT_EQ(rtc_stored.data.boot_count, 55);
    EXPECT_EQ(nvs_stored.data.boot_count, 55);
}

TEST_F(NvsCoreTest, SaveToNvsFailsWhenNvsError)
{
    // Arrange
    CoreData to_save = CreateValidCoreData();
    EXPECT_CALL(nvs_backend_, save).WillOnce(Return(ESP_ERR_NVS_NOT_INITIALIZED));

    // Act
    esp_err_t ret = nvs_core_->save_core(to_save, true);

    // Assert
    EXPECT_EQ(ret, ESP_ERR_NVS_NOT_INITIALIZED);
}

TEST_F(NvsCoreTest, RoundTripSaveAndLoad)
{
    // Arrange
    CoreData original = CreateValidCoreData();
    original.boot_count = 123;
    original.crash_count = 5;

    // Act: Save and load back
    esp_err_t save_ret = nvs_core_->save_core(original, true);
    EXPECT_EQ(save_ret, ESP_OK);

    CoreData loaded{};
    esp_err_t load_ret = nvs_core_->load_core(loaded);
    EXPECT_EQ(load_ret, ESP_OK);

    // Assert
    EXPECT_EQ(loaded.boot_count, 123);
    EXPECT_EQ(loaded.crash_count, 5);
    EXPECT_EQ(loaded.node_id, original.node_id);
}

TEST_F(NvsCoreTest, RejectDataWithBadCrc)
{
    // Arrange
    CoreStorage corrupt = CreateValidCoreEnvelope();
    corrupt.crc = 0xDEADBEEF;
    SetNvsEnvelope(corrupt);

    // Act
    CoreData loaded{};
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

TEST_F(NvsCoreTest, RejectDataWithWrongMagic)
{
    // Arrange
    CoreStorage bad_magic = CreateValidCoreEnvelope();
    bad_magic.magic = 0xDEADBEEF;
    bad_magic.crc = test_calculate_crc(bad_magic);
    SetNvsEnvelope(bad_magic);

    // Act
    CoreData loaded{};
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

TEST_F(NvsCoreTest, RejectDataWithWrongVersion)
{
    // Arrange
    CoreStorage bad_version = CreateValidCoreEnvelope();
    bad_version.version = 10;
    bad_version.crc = test_calculate_crc(bad_version);
    SetNvsEnvelope(bad_version);

    // Act
    CoreData loaded{};
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

TEST_F(NvsCoreTest, InitCreatesDefaultWhenStorageEmpty)
{
    CoreData default_core;
    default_core.node_id = farm::NodeId::HUB;
    default_core.node_type = farm::NodeType::HUB;
    default_core.power_profile = farm::PowerProfile::ALWAYS_ON;

    CoreData core{};

    esp_err_t ret = nvs_core_->init(core, default_core);

    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(core.node_id, farm::NodeId::HUB);
    EXPECT_EQ(core.node_type, farm::NodeType::HUB);
    EXPECT_EQ(core.power_profile, farm::PowerProfile::ALWAYS_ON);

    // Verify it was stored in NVS
    CoreStorage nvs_stored = GetStoredNvsEnvelope();
    EXPECT_EQ(nvs_stored.data.node_id, farm::NodeId::HUB);
}

TEST_F(NvsCoreTest, ProcessBootReasonsPowerOn)
{
    CoreData core{};
    core.boot_count = 5;
    bool pending_commit = false;

    nvs_core_->process_boot_reasons(core, ESP_RST_POWERON, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 6);
    EXPECT_EQ(core.crash_count, 0);
    EXPECT_EQ(core.last_wake, WakeSource::POWER_ON);
    EXPECT_FALSE(pending_commit);
}

TEST_F(NvsCoreTest, ProcessBootReasonsCrash)
{
    CoreData core{};
    core.boot_count = 10;
    core.crash_count = 1;
    bool pending_commit = false;

    nvs_core_->process_boot_reasons(core, ESP_RST_PANIC, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 11);
    EXPECT_EQ(core.crash_count, 2);
    EXPECT_EQ(core.last_wake, WakeSource::CRASH);
    EXPECT_TRUE(pending_commit);
}

TEST_F(NvsCoreTest, ProcessBootReasonsSoftwareRestart)
{
    CoreData core{};
    core.boot_count = 3;
    bool pending_commit = false;

    nvs_core_->process_boot_reasons(core, ESP_RST_SW, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 4);
    EXPECT_EQ(core.last_wake, WakeSource::RESTART);
    EXPECT_FALSE(pending_commit);
}

TEST_F(NvsCoreTest, ProcessBootReasonsDeepSleepTimer)
{
    CoreData core{};
    core.boot_count = 20;
    bool pending_commit = false;

    nvs_core_->process_boot_reasons(core, ESP_RST_DEEPSLEEP, ESP_SLEEP_WAKEUP_TIMER, pending_commit);

    EXPECT_EQ(core.boot_count, 21);
    EXPECT_EQ(core.last_wake, WakeSource::TIMER);
    EXPECT_FALSE(pending_commit);
}

TEST_F(NvsCoreTest, ProcessBootReasonsDeepSleepGpio)
{
    CoreData core{};
    core.boot_count = 20;
    bool pending_commit = false;

    nvs_core_->process_boot_reasons(core, ESP_RST_DEEPSLEEP, ESP_SLEEP_WAKEUP_GPIO, pending_commit);

    EXPECT_EQ(core.boot_count, 21);
    EXPECT_EQ(core.last_wake, WakeSource::GPIO);
    EXPECT_FALSE(pending_commit);
}

TEST_F(NvsCoreTest, ProcessBootReasonsUnknown)
{
    CoreData core{};
    core.boot_count = 100;
    bool pending_commit = false;

    nvs_core_->process_boot_reasons(core, ESP_RST_UNKNOWN, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 101);
    EXPECT_EQ(core.last_wake, WakeSource::UNKNOWN);
    EXPECT_FALSE(pending_commit);
}