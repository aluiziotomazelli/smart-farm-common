#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esp_err.h"
#include "esp_rom_crc.h"

#include "mock_persistence_backend.hpp"
#include "app_storage.hpp"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/**
 * @brief Dummy data struct for generic AppStorage tests.
 */
struct DummyStats
{
    static constexpr uint16_t MAGIC = 0xA1B2;
    static constexpr uint8_t VERSION = 1;

    uint16_t magic = MAGIC;
    uint8_t version = VERSION;
    uint32_t counter = 0;
    int32_t value = 0;
    bool flag = false;

    // CRC must be present and not first
    uint32_t crc = 0;

    void reset()
    {
        *this = {};
        magic = MAGIC;
        version = VERSION;
    }

    bool operator==(const DummyStats& other) const
    {
        return magic == other.magic && version == other.version && counter == other.counter &&
               value == other.value && flag == other.flag && crc == other.crc;
    }

    bool operator!=(const DummyStats& other) const { return !(*this == other); }
};

/**
 * @brief Concrete AppStorage implementation using DummyStats.
 */
class DummyAppStorage : public AppStorage<DummyStats>
{
public:
    DummyAppStorage(IPersistenceBackend& rtc, IPersistenceBackend& nvs)
        : AppStorage<DummyStats>(rtc, nvs, "DummyAppStorage")
    {
    }

    esp_err_t init_app_data(DummyStats& stats, const DummyStats& default_stats)
    {
        return init_app_data_impl(stats, default_stats);
    }

    esp_err_t load_app_data(DummyStats& stats) { return load_app_data_impl(stats); }

    esp_err_t save_app_data(const DummyStats& stats, bool force_nvs_commit = false)
    {
        return save_app_data_impl(stats, force_nvs_commit);
    }
};

/**
 * @brief Helper to compute CRC for tests.
 */
template <typename T> inline uint32_t test_calculate_crc(const T& data)
{
    static_assert(std::is_standard_layout_v<T>, "T must be standard_layout for safe offset calculation");
    static_assert(offsetof(T, crc) != 0, "T must have a non-first crc field");

    return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&data), offsetof(T, crc));
}

/**
 * @brief Test fixture for AppStorage generic tests.
 */
class AppStorageTest : public ::testing::Test
{
protected:
    NiceMock<MockPersistenceBackend> rtc_backend_;
    NiceMock<MockPersistenceBackend> nvs_backend_;

    std::unique_ptr<DummyAppStorage> sut_;

    void SetUp() override
    {
        rtc_backend_.UseRealStorage();
        nvs_backend_.UseRealStorage();

        sut_ = std::make_unique<DummyAppStorage>(rtc_backend_, nvs_backend_);
    }

    void TearDown() override { sut_.reset(); }

    DummyStats CreateValidStats(uint32_t counter = 42, int32_t value = -100, bool flag = true)
    {
        DummyStats stats;
        stats.magic = DummyStats::MAGIC;
        stats.version = DummyStats::VERSION;
        stats.counter = counter;
        stats.value = value;
        stats.flag = flag;
        stats.crc = test_calculate_crc(stats);
        return stats;
    }

    void SetRtcData(const DummyStats& stats) { rtc_backend_.save(&stats, sizeof(stats)); }

    void SetNvsData(const DummyStats& stats) { nvs_backend_.save(&stats, sizeof(stats)); }

    DummyStats GetStoredRtcData() const
    {
        DummyStats stats{};
        memcpy(&stats, rtc_backend_.GetStoredData(), sizeof(stats));
        return stats;
    }

    DummyStats GetStoredNvsData() const
    {
        DummyStats stats{};
        memcpy(&stats, nvs_backend_.GetStoredData(), sizeof(stats));
        return stats;
    }
};

// =============================================================
// Test Cases
// =============================================================

TEST_F(AppStorageTest, InitLoadsExistingData)
{
    // Arrange: valid data already exists in RTC
    DummyStats existing = CreateValidStats(100, 200, true);
    SetRtcData(existing);

    DummyStats default_stats;
    default_stats.reset();
    default_stats.counter = 999;

    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->init_app_data(loaded, default_stats);

    // Assert: should load existing, not overwrite with default
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 100);
    EXPECT_EQ(loaded.value, 200);
    EXPECT_TRUE(loaded.flag);
}

TEST_F(AppStorageTest, InitCreatesDefaultWhenLoadFails)
{
    // Arrange: storage is completely empty
    DummyStats default_stats = CreateValidStats(555, -999, true);
    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->init_app_data(loaded, default_stats);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 555);
    EXPECT_EQ(loaded.value, -999);
    EXPECT_TRUE(loaded.flag);

    // Both RTC and NVS should now contain default data with valid CRC
    DummyStats rtc_stored = GetStoredRtcData();
    DummyStats nvs_stored = GetStoredNvsData();

    EXPECT_EQ(rtc_stored.counter, 555);
    EXPECT_EQ(nvs_stored.counter, 555);
    EXPECT_EQ(rtc_stored.crc, test_calculate_crc(rtc_stored));
    EXPECT_EQ(nvs_stored.crc, test_calculate_crc(nvs_stored));
}

TEST_F(AppStorageTest, LoadFromRtcWhenValid)
{
    // Arrange
    DummyStats expected = CreateValidStats(10, 20, false);
    SetRtcData(expected);

    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 10);
    EXPECT_EQ(loaded.value, 20);
    EXPECT_FALSE(loaded.flag);
    EXPECT_EQ(loaded.crc, expected.crc);
}

TEST_F(AppStorageTest, FallbackToNvsWhenRtcCorruptedMagic)
{
    // Arrange: corrupted RTC magic, valid NVS
    DummyStats rtc_corrupted = CreateValidStats(1, 1, false);
    rtc_corrupted.magic = 0xDEAD;
    rtc_corrupted.crc = test_calculate_crc(rtc_corrupted);
    SetRtcData(rtc_corrupted);

    DummyStats nvs_valid = CreateValidStats(77, 88, true);
    SetNvsData(nvs_valid);

    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert: should fall back to NVS
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 77);
    EXPECT_EQ(loaded.value, 88);
}

TEST_F(AppStorageTest, FallbackToNvsWhenRtcCorruptedVersion)
{
    // Arrange: corrupted RTC version, valid NVS
    DummyStats rtc_corrupted = CreateValidStats(1, 1, false);
    rtc_corrupted.version = 99;
    rtc_corrupted.crc = test_calculate_crc(rtc_corrupted);
    SetRtcData(rtc_corrupted);

    DummyStats nvs_valid = CreateValidStats(123, 456, true);
    SetNvsData(nvs_valid);

    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert: should fall back to NVS
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 123);
    EXPECT_EQ(loaded.value, 456);
}

TEST_F(AppStorageTest, FallbackToNvsWhenRtcCorruptedCrc)
{
    // Arrange: corrupted RTC CRC, valid NVS
    DummyStats rtc_corrupted = CreateValidStats(1, 1, false);
    rtc_corrupted.crc = 0x12345678; // wrong CRC
    SetRtcData(rtc_corrupted);

    DummyStats nvs_valid = CreateValidStats(321, 654, false);
    SetNvsData(nvs_valid);

    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert: should fall back to NVS
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 321);
    EXPECT_EQ(loaded.value, 654);
}

TEST_F(AppStorageTest, SyncBackToRtcAfterNvsLoad)
{
    // Arrange: RTC is empty, NVS has valid data
    DummyStats nvs_valid = CreateValidStats(999, -1, true);
    SetNvsData(nvs_valid);

    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 999);

    // RTC should now be populated with the valid data
    DummyStats rtc_synced = GetStoredRtcData();
    EXPECT_EQ(rtc_synced.counter, 999);
    EXPECT_EQ(rtc_synced.crc, test_calculate_crc(rtc_synced));
}

TEST_F(AppStorageTest, ReturnErrorWhenBothBackendsFail)
{
    // Arrange: neither RTC nor NVS has valid data
    DummyStats loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

TEST_F(AppStorageTest, SaveSkipsWhenDataNotDirty)
{
    // Arrange: RTC already contains exact same data
    DummyStats stats = CreateValidStats(50, 60, true);
    SetRtcData(stats);

    // Mock save call on RTC to expect 0 calls
    EXPECT_CALL(rtc_backend_, save(_, _)).Times(0);
    EXPECT_CALL(nvs_backend_, save(_, _)).Times(0);

    // Act: saving identical data with force=false
    esp_err_t ret = sut_->save_app_data(stats, /*force_nvs_commit=*/false);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
}

TEST_F(AppStorageTest, SaveWritesRtcWhenDirty)
{
    // Arrange: RTC has old data
    DummyStats old_stats = CreateValidStats(1, 1, false);
    SetRtcData(old_stats);

    DummyStats new_stats = CreateValidStats(2, 2, true);

    // Expect RTC save to be called, but NVS save NOT called
    EXPECT_CALL(rtc_backend_, save(_, sizeof(DummyStats))).Times(1);
    EXPECT_CALL(nvs_backend_, save(_, _)).Times(0);

    // Act
    esp_err_t ret = sut_->save_app_data(new_stats, /*force_nvs_commit=*/false);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    DummyStats stored_rtc = GetStoredRtcData();
    EXPECT_EQ(stored_rtc.counter, 2);
    EXPECT_EQ(stored_rtc.crc, test_calculate_crc(stored_rtc));
}

TEST_F(AppStorageTest, SaveWritesNvsWhenForced)
{
    // Arrange
    DummyStats stats = CreateValidStats(100, 200, true);

    // Expect NVS save to be called
    EXPECT_CALL(nvs_backend_, save(_, sizeof(DummyStats))).Times(1);

    // Act
    esp_err_t ret = sut_->save_app_data(stats, /*force_nvs_commit=*/true);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    DummyStats stored_nvs = GetStoredNvsData();
    EXPECT_EQ(stored_nvs.counter, 100);
    EXPECT_EQ(stored_nvs.crc, test_calculate_crc(stored_nvs));
}
