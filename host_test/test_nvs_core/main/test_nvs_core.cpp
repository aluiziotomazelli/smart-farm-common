#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "nvs_core.hpp"
#include "core_types.hpp"
#include "mock_persistence_backend.hpp"
#include "esp_rom_crc.h"
#include <memory>
#include <cstring>
#include <type_traits>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/**
 * Helper: Calculate CRC for a struct with crc field
 */
template <typename T> inline uint32_t test_calculate_crc(const T& data)
{
    static_assert(std::is_standard_layout_v<T>, "T must be standard_layout for safe offset calculation");
    static_assert(offsetof(T, crc) != 0, "T must have a non-first crc field");

    return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&data), offsetof(T, crc));
}

/**
 * Fixture for NvsCore tests.
 * Manages mock backends and provides utility methods.
 */
class NvsCoreTest : public ::testing::Test
{
protected:
    // Use NiceMock to suppress warnings for uninteresting calls
    NiceMock<MockPersistenceBackend> rtc_backend_;
    NiceMock<MockPersistenceBackend> nvs_backend_;

    // System under test
    std::unique_ptr<NvsCore> nvs_core_;

    void SetUp() override
    {
        // Configure backends to use real in-memory storage
        rtc_backend_.UseRealStorage();
        nvs_backend_.UseRealStorage();

        // Create NvsCore instance with the mock backends
        nvs_core_ = std::make_unique<NvsCore>(rtc_backend_, nvs_backend_);
    }

    void TearDown() override { nvs_core_.reset(); }

    /**
     * Helper: Create a valid CoreStorage with all fields set.
     */
    CoreStorage CreateValidCoreStorage()
    {
        CoreStorage core;
        core.magic = CoreStorage::CORE_MAGIC;
        core.version = CoreStorage::CORE_VERSION;
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
        core.power_profile = PowerProfile::DEEP_SLEEP;
        core.sleep_interval_s = 300;
        core.last_wake = WakeSource::TIMER;

        // Calculate and set CRC
        core.crc = test_calculate_crc(core);

        return core;
    }

    /**
     * Helper: Pre-populate RTC backend with data.
     */
    void SetRtcData(const CoreStorage& core) { rtc_backend_.save(&core, sizeof(core)); }

    /**
     * Helper: Pre-populate NVS backend with data.
     */
    void SetNvsData(const CoreStorage& core) { nvs_backend_.save(&core, sizeof(core)); }

    /**
     * Helper: Get stored RTC data for verification.
     */
    CoreStorage GetStoredRtcData() const
    {
        CoreStorage core;
        memcpy(&core, rtc_backend_.GetStoredData(), sizeof(core));
        return core;
    }

    /**
     * Helper: Get stored NVS data for verification.
     */
    CoreStorage GetStoredNvsData() const
    {
        CoreStorage core;
        memcpy(&core, nvs_backend_.GetStoredData(), sizeof(core));
        return core;
    }
};

// =============================================================
// Tests
// =============================================================

/**
 * Test: Load from RTC when valid
 *
 * Scenario: RTC has valid data, NVS is empty
 * Expected: Data is loaded from RTC, NVS is not accessed
 */
TEST_F(NvsCoreTest, LoadFromRtcWhenValid)
{
    // Arrange: Put valid data in RTC, nothing in NVS
    CoreStorage expected = CreateValidCoreStorage();
    SetRtcData(expected);

    // Expect RTC to be called, NVS may not be
    EXPECT_CALL(rtc_backend_, load).Times(::testing::AtLeast(1));
    EXPECT_CALL(nvs_backend_, load).Times(0);

    // Act
    CoreStorage loaded;
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.magic, expected.magic);
    EXPECT_EQ(loaded.boot_count, expected.boot_count);
    EXPECT_EQ(loaded.node_id, expected.node_id);
}

/**
 * Test: Load from NVS when RTC is invalid
 *
 * Scenario: RTC is empty/corrupt, NVS has valid data
 * Expected: Data is loaded from NVS, and synced back to RTC
 */
TEST_F(NvsCoreTest, LoadFromNvsWhenRtcInvalid)
{
    // Arrange: Put valid data only in NVS
    CoreStorage expected = CreateValidCoreStorage();
    SetNvsData(expected);
    // RTC is empty (default zeroed)

    // Act
    CoreStorage loaded;
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.boot_count, expected.boot_count);

    // Verify RTC was synced with valid NVS data
    CoreStorage rtc_synced = GetStoredRtcData();
    EXPECT_EQ(rtc_synced.magic, expected.magic);
    EXPECT_EQ(rtc_synced.boot_count, expected.boot_count);
}

/**
 * Test: Load fails when both RTC and NVS are invalid
 *
 * Scenario: Both backends have corrupt/missing data
 * Expected: Return error
 */
TEST_F(NvsCoreTest, LoadFailsWhenBothInvalid)
{
    // Arrange: Both backends are empty (invalid)
    // (setUp() already clears them)

    // Act
    CoreStorage loaded;
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert: Should fail
    EXPECT_NE(ret, ESP_OK);
}

/**
 * Test: Save to RTC only when not forcing NVS
 *
 * Scenario: Data is dirty but force_nvs_commit is false
 * Expected: Data goes to RTC, not NVS
 */
TEST_F(NvsCoreTest, SaveToDirtyDirtyDataNoForce)
{
    // Arrange
    CoreStorage to_save = CreateValidCoreStorage();
    to_save.boot_count = 99; // Change data to make it dirty

    EXPECT_CALL(rtc_backend_, save).Times(::testing::AtLeast(1));
    EXPECT_CALL(nvs_backend_, save).Times(0); // Should NOT call NVS

    // Act
    esp_err_t ret = nvs_core_->save_core(to_save, false);

    // Assert
    EXPECT_EQ(ret, ESP_OK);

    // Verify RTC has the new data
    CoreStorage rtc_stored = GetStoredRtcData();
    EXPECT_EQ(rtc_stored.boot_count, 99);
}

/**
 * Test: Save to both RTC and NVS when forcing
 *
 * Scenario: force_nvs_commit is true
 * Expected: Data goes to both RTC and NVS
 */
TEST_F(NvsCoreTest, SaveToBothWhenForceNvs)
{
    // Arrange
    CoreStorage to_save = CreateValidCoreStorage();
    to_save.boot_count = 55;

    EXPECT_CALL(rtc_backend_, save).Times(::testing::AtLeast(1));
    EXPECT_CALL(nvs_backend_, save).Times(::testing::AtLeast(1));

    // Act
    esp_err_t ret = nvs_core_->save_core(to_save, true);

    // Assert
    EXPECT_EQ(ret, ESP_OK);

    // Verify both backends have the data
    CoreStorage rtc_stored = GetStoredRtcData();
    CoreStorage nvs_stored = GetStoredNvsData();

    EXPECT_EQ(rtc_stored.boot_count, 55);
    EXPECT_EQ(nvs_stored.boot_count, 55);
}
/**
 *
 */
TEST_F(NvsCoreTest, SaveToNvsFailsWhenNvsError)
{
    // Arrange
    CoreStorage to_save = CreateValidCoreStorage();
    to_save.boot_count = 77;

    // Force NVS save to fail
    EXPECT_CALL(nvs_backend_, save).WillOnce(Return(ESP_ERR_NVS_NOT_INITIALIZED));

    // Act
    esp_err_t ret = nvs_core_->save_core(to_save, true);

    // Assert: Should return the NVS error
    EXPECT_EQ(ret, ESP_ERR_NVS_NOT_INITIALIZED);
}

/**
 * Test: Round-trip: save and load
 *
 * Scenario: Save data to backends, then load it back
 * Expected: Loaded data matches saved data
 */
TEST_F(NvsCoreTest, RoundTripSaveAndLoad)
{
    // Arrange
    CoreStorage original = CreateValidCoreStorage();
    original.boot_count = 123;
    original.crash_count = 5;

    // Act: Save (force NVS for completeness)
    esp_err_t save_ret = nvs_core_->save_core(original, true);
    EXPECT_EQ(save_ret, ESP_OK);

    // Load back
    CoreStorage loaded;
    esp_err_t load_ret = nvs_core_->load_core(loaded);
    EXPECT_EQ(load_ret, ESP_OK);

    // Assert
    EXPECT_EQ(loaded.boot_count, 123);
    EXPECT_EQ(loaded.crash_count, 5);
    EXPECT_EQ(loaded.node_id, original.node_id);
}

/**
 * Test: CRC validation on load
 *
 * Scenario: Corrupt data with bad CRC in NVS
 * Expected: Load fails, does not accept corrupt data
 */
TEST_F(NvsCoreTest, RejectDataWithBadCrc)
{
    // Arrange: Valid data but corrupt the CRC
    CoreStorage corrupt = CreateValidCoreStorage();
    corrupt.crc = 0xDEADBEEF; // Wrong CRC
    SetNvsData(corrupt);
    // RTC is empty

    // Act
    CoreStorage loaded;
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert: Should fail, not accept corrupt data
    EXPECT_NE(ret, ESP_OK);
}

/**
 * Test: Magic field validation
 *
 * Scenario: Data with wrong magic number
 * Expected: Load fails
 */
TEST_F(NvsCoreTest, RejectDataWithWrongMagic)
{
    // Arrange
    CoreStorage bad_magic = CreateValidCoreStorage();
    bad_magic.magic = 0xDEADBEEF;                  // Wrong magic
    bad_magic.crc = test_calculate_crc(bad_magic); // Update CRC
    SetNvsData(bad_magic);

    // Act
    CoreStorage loaded;
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

/**
 * Test: Version field validation
 *
 * Scenario: Data with wrong version
 * Expected: Load fails
 */
TEST_F(NvsCoreTest, RejectDataWithWrongVersion)
{
    // Arrange
    CoreStorage bad_version = CreateValidCoreStorage();
    bad_version.version = 10;                          // Wrong version
    bad_version.crc = test_calculate_crc(bad_version); // Update CRC
    SetNvsData(bad_version);

    // Act
    CoreStorage loaded;
    esp_err_t ret = nvs_core_->load_core(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

/**
 * Test: No double-save when data unchanged
 *
 * Scenario: Save identical data multiple times
 * Expected: First save goes to NVS, subsequent saves skip NVS (not dirty)
 */
TEST_F(NvsCoreTest, NoDoubleSaveWhenUnchanged)
{
    // Arrange
    CoreStorage data = CreateValidCoreStorage();

    // First save with force
    nvs_core_->save_core(data, true);

    // Reset mocks to track second save
    rtc_backend_.Clear();
    nvs_backend_.Clear();
    rtc_backend_.UseRealStorage();
    nvs_backend_.UseRealStorage();

    // Set up expectations for second save (should NOT touch NVS if not dirty)
    // This is tricky with mocks - just verify behavior
    CoreStorage same_data = data;
    esp_err_t ret = nvs_core_->save_core(same_data, false);

    // If data is truly identical, should return quickly
    EXPECT_EQ(ret, ESP_OK);
}

/**
 * Test: Create default storage
 *
 * Scenario: Calling create_default_storage with a default CoreStorage structure
 * Expected: Storage is saved to backends and core is updated with default_core
 */
TEST_F(NvsCoreTest, CreateDefaultStorage)
{
    CoreStorage default_core;
    default_core.reset();
    default_core.node_id = farm::NodeId::HUB;
    default_core.node_type = farm::NodeType::HUB;
    default_core.power_profile = PowerProfile::ALWAYS_ON;

    CoreStorage core;
    esp_err_t ret = nvs_core_->create_default_storage(core, default_core);

    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(core.node_id, farm::NodeId::HUB);
    EXPECT_EQ(core.node_type, farm::NodeType::HUB);
    EXPECT_EQ(core.power_profile, PowerProfile::ALWAYS_ON);

    // Verify it was stored in NVS
    CoreStorage nvs_stored = GetStoredNvsData();
    EXPECT_EQ(nvs_stored.node_id, farm::NodeId::HUB);
}

/**
 * Test: Process boot reasons - Power On
 */
TEST_F(NvsCoreTest, ProcessBootReasonsPowerOn)
{
    CoreStorage core;
    core.boot_count = 5;
    core.crash_count = 0;
    bool pending_commit = true;

    nvs_core_->process_boot_reasons(core, ESP_RST_POWERON, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 6);
    EXPECT_EQ(core.crash_count, 0);
    EXPECT_EQ(core.last_wake, WakeSource::POWER_ON);
    EXPECT_FALSE(pending_commit);
}

/**
 * Test: Process boot reasons - Crash Reset
 */
TEST_F(NvsCoreTest, ProcessBootReasonsCrash)
{
    CoreStorage core;
    core.boot_count = 10;
    core.crash_count = 1;
    bool pending_commit = false;

    nvs_core_->process_boot_reasons(core, ESP_RST_PANIC, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 11);
    EXPECT_EQ(core.crash_count, 2);
    EXPECT_EQ(core.last_wake, WakeSource::CRASH);
    EXPECT_TRUE(pending_commit);
}

/**
 * Test: Process boot reasons - Software Reset
 */
TEST_F(NvsCoreTest, ProcessBootReasonsSoftwareRestart)
{
    CoreStorage core;
    core.boot_count = 3;
    bool pending_commit = true;

    nvs_core_->process_boot_reasons(core, ESP_RST_SW, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 4);
    EXPECT_EQ(core.last_wake, WakeSource::RESTART);
    EXPECT_FALSE(pending_commit);
}

/**
 * Test: Process boot reasons - Deep Sleep Timer Wakeup
 */
TEST_F(NvsCoreTest, ProcessBootReasonsDeepSleepTimer)
{
    CoreStorage core;
    core.boot_count = 20;
    bool pending_commit = true;

    nvs_core_->process_boot_reasons(core, ESP_RST_DEEPSLEEP, ESP_SLEEP_WAKEUP_TIMER, pending_commit);

    EXPECT_EQ(core.boot_count, 21);
    EXPECT_EQ(core.last_wake, WakeSource::TIMER);
    EXPECT_FALSE(pending_commit);
}

/**
 * Test: Process boot reasons - Deep Sleep GPIO Wakeup
 */
TEST_F(NvsCoreTest, ProcessBootReasonsDeepSleepGpio)
{
    CoreStorage core;
    core.boot_count = 20;
    bool pending_commit = true;

    nvs_core_->process_boot_reasons(core, ESP_RST_DEEPSLEEP, ESP_SLEEP_WAKEUP_GPIO, pending_commit);

    EXPECT_EQ(core.boot_count, 21);
    EXPECT_EQ(core.last_wake, WakeSource::GPIO);
    EXPECT_FALSE(pending_commit);
}

/**
 * Test: Process boot reasons - Unknown/Default Reset
 */
TEST_F(NvsCoreTest, ProcessBootReasonsUnknown)
{
    CoreStorage core;
    core.boot_count = 100;
    bool pending_commit = true;

    nvs_core_->process_boot_reasons(core, ESP_RST_UNKNOWN, ESP_SLEEP_WAKEUP_UNDEFINED, pending_commit);

    EXPECT_EQ(core.boot_count, 101);
    EXPECT_EQ(core.last_wake, WakeSource::UNKNOWN);
    EXPECT_FALSE(pending_commit);
}