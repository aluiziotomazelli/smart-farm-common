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

static constexpr uint32_t DUMMY_MAGIC = 0xA1B2C3D4;
static constexpr uint8_t DUMMY_VERSION = 1;

/**
 * @brief Pure dummy domain struct (no storage metadata).
 */
struct DummyData
{
    uint32_t counter = 0;
    int32_t value = 0;
    bool flag = false;

    void reset() { *this = {}; }

    bool operator==(const DummyData& other) const
    {
        return counter == other.counter && value == other.value && flag == other.flag;
    }

    bool operator!=(const DummyData& other) const { return !(*this == other); }
};

using DummyEnvelope = StorageEnvelope<DummyData, DUMMY_MAGIC, DUMMY_VERSION>;

/**
 * @brief Concrete AppStorage implementation using DummyData.
 */
class DummyAppStorage : public AppStorage<DummyData, DUMMY_MAGIC, DUMMY_VERSION>
{
public:
    DummyAppStorage(IPersistenceBackend& rtc, IPersistenceBackend& nvs)
        : AppStorage<DummyData, DUMMY_MAGIC, DUMMY_VERSION>(rtc, nvs, "DummyAppStorage")
    {
    }

    esp_err_t init_app_data(DummyData& data, const DummyData& default_data)
    {
        return init_app_data_impl(data, default_data);
    }

    esp_err_t load_app_data(DummyData& data) { return load_app_data_impl(data); }

    esp_err_t save_app_data(const DummyData& data, bool force_nvs_commit = false)
    {
        return save_app_data_impl(data, force_nvs_commit);
    }
};

/**
 * @brief Helper to compute CRC for envelope in tests.
 */
template <typename T> inline uint32_t test_calculate_crc(const T& envelope)
{
    static_assert(std::is_standard_layout_v<T>, "T must be standard_layout for safe offset calculation");
    static_assert(offsetof(T, crc) != 0, "T must have a non-first crc field");

    return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&envelope), offsetof(T, crc));
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

    DummyEnvelope CreateValidEnvelope(uint32_t counter = 42, int32_t value = -100, bool flag = true)
    {
        DummyEnvelope env;
        env.magic = DUMMY_MAGIC;
        env.version = DUMMY_VERSION;
        env.data.counter = counter;
        env.data.value = value;
        env.data.flag = flag;
        env.crc = test_calculate_crc(env);
        return env;
    }

    void SetRtcEnvelope(const DummyEnvelope& env) { rtc_backend_.save(&env, sizeof(env)); }

    void SetNvsEnvelope(const DummyEnvelope& env) { nvs_backend_.save(&env, sizeof(env)); }

    DummyEnvelope GetStoredRtcEnvelope() const
    {
        DummyEnvelope env{};
        memcpy(&env, rtc_backend_.GetStoredData(), sizeof(env));
        return env;
    }

    DummyEnvelope GetStoredNvsEnvelope() const
    {
        DummyEnvelope env{};
        memcpy(&env, nvs_backend_.GetStoredData(), sizeof(env));
        return env;
    }
};

// =============================================================
// Test Cases
// =============================================================

TEST_F(AppStorageTest, InitLoadsExistingData)
{
    // Arrange: valid envelope already exists in RTC
    DummyEnvelope existing = CreateValidEnvelope(100, 200, true);
    SetRtcEnvelope(existing);

    DummyData default_data;
    default_data.counter = 999;

    DummyData loaded{};

    // Act
    esp_err_t ret = sut_->init_app_data(loaded, default_data);

    // Assert: should load existing, not overwrite with default
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 100);
    EXPECT_EQ(loaded.value, 200);
    EXPECT_TRUE(loaded.flag);
}

TEST_F(AppStorageTest, InitCreatesDefaultWhenLoadFails)
{
    // Arrange: storage is completely empty
    DummyData default_data{555, -999, true};
    DummyData loaded{};

    // Act
    esp_err_t ret = sut_->init_app_data(loaded, default_data);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 555);
    EXPECT_EQ(loaded.value, -999);
    EXPECT_TRUE(loaded.flag);

    // Both RTC and NVS should now contain default data wrapped in valid envelope
    DummyEnvelope rtc_stored = GetStoredRtcEnvelope();
    DummyEnvelope nvs_stored = GetStoredNvsEnvelope();

    EXPECT_EQ(rtc_stored.data.counter, 555);
    EXPECT_EQ(nvs_stored.data.counter, 555);
    EXPECT_EQ(rtc_stored.crc, test_calculate_crc(rtc_stored));
    EXPECT_EQ(nvs_stored.crc, test_calculate_crc(nvs_stored));
}

TEST_F(AppStorageTest, LoadFromRtcWhenValid)
{
    // Arrange
    DummyEnvelope expected = CreateValidEnvelope(10, 20, false);
    SetRtcEnvelope(expected);

    DummyData loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 10);
    EXPECT_EQ(loaded.value, 20);
    EXPECT_FALSE(loaded.flag);
}

TEST_F(AppStorageTest, FallbackToNvsWhenRtcCorruptedMagic)
{
    // Arrange: corrupted RTC magic, valid NVS
    DummyEnvelope rtc_corrupted = CreateValidEnvelope(1, 1, false);
    rtc_corrupted.magic = 0xDEADBEEF;
    rtc_corrupted.crc = test_calculate_crc(rtc_corrupted);
    SetRtcEnvelope(rtc_corrupted);

    DummyEnvelope nvs_valid = CreateValidEnvelope(77, 88, true);
    SetNvsEnvelope(nvs_valid);

    DummyData loaded{};

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
    DummyEnvelope rtc_corrupted = CreateValidEnvelope(1, 1, false);
    rtc_corrupted.version = 99;
    rtc_corrupted.crc = test_calculate_crc(rtc_corrupted);
    SetRtcEnvelope(rtc_corrupted);

    DummyEnvelope nvs_valid = CreateValidEnvelope(123, 456, true);
    SetNvsEnvelope(nvs_valid);

    DummyData loaded{};

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
    DummyEnvelope rtc_corrupted = CreateValidEnvelope(1, 1, false);
    rtc_corrupted.crc = 0x12345678; // wrong CRC
    SetRtcEnvelope(rtc_corrupted);

    DummyEnvelope nvs_valid = CreateValidEnvelope(321, 654, false);
    SetNvsEnvelope(nvs_valid);

    DummyData loaded{};

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
    DummyEnvelope nvs_valid = CreateValidEnvelope(999, -1, true);
    SetNvsEnvelope(nvs_valid);

    DummyData loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    EXPECT_EQ(loaded.counter, 999);

    // RTC should now be populated with the valid data
    DummyEnvelope rtc_synced = GetStoredRtcEnvelope();
    EXPECT_EQ(rtc_synced.data.counter, 999);
    EXPECT_EQ(rtc_synced.crc, test_calculate_crc(rtc_synced));
}

TEST_F(AppStorageTest, ReturnErrorWhenBothBackendsFail)
{
    // Arrange: neither RTC nor NVS has valid data
    DummyData loaded{};

    // Act
    esp_err_t ret = sut_->load_app_data(loaded);

    // Assert
    EXPECT_NE(ret, ESP_OK);
}

TEST_F(AppStorageTest, SaveSkipsWhenDataNotDirty)
{
    // Arrange: RTC already contains exact same data
    DummyEnvelope env = CreateValidEnvelope(50, 60, true);
    SetRtcEnvelope(env);

    // Mock save call on RTC to expect 0 calls
    EXPECT_CALL(rtc_backend_, save(_, _)).Times(0);
    EXPECT_CALL(nvs_backend_, save(_, _)).Times(0);

    // Act: saving identical data with force=false
    DummyData same_data{50, 60, true};
    esp_err_t ret = sut_->save_app_data(same_data, /*force_nvs_commit=*/false);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
}

TEST_F(AppStorageTest, SaveWritesRtcWhenDirty)
{
    // Arrange: RTC has old data
    DummyEnvelope old_env = CreateValidEnvelope(1, 1, false);
    SetRtcEnvelope(old_env);

    DummyData new_data{2, 2, true};

    // Expect RTC save to be called, but NVS save NOT called
    EXPECT_CALL(rtc_backend_, save(_, sizeof(DummyEnvelope))).Times(1);
    EXPECT_CALL(nvs_backend_, save(_, _)).Times(0);

    // Act
    esp_err_t ret = sut_->save_app_data(new_data, /*force_nvs_commit=*/false);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    DummyEnvelope stored_rtc = GetStoredRtcEnvelope();
    EXPECT_EQ(stored_rtc.data.counter, 2);
    EXPECT_EQ(stored_rtc.crc, test_calculate_crc(stored_rtc));
}

TEST_F(AppStorageTest, SaveWritesNvsWhenForced)
{
    // Arrange
    DummyData data{100, 200, true};

    // Expect NVS save to be called
    EXPECT_CALL(nvs_backend_, save(_, sizeof(DummyEnvelope))).Times(1);

    // Act
    esp_err_t ret = sut_->save_app_data(data, /*force_nvs_commit=*/true);

    // Assert
    EXPECT_EQ(ret, ESP_OK);
    DummyEnvelope stored_nvs = GetStoredNvsEnvelope();
    EXPECT_EQ(stored_nvs.data.counter, 100);
    EXPECT_EQ(stored_nvs.crc, test_calculate_crc(stored_nvs));
}
