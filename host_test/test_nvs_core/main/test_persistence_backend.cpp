#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "esp_attr.h"
#include "mock_hal_nvs.hpp"

#include "persistence_backend.hpp"
#include "core_types.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

// ==============================================================================
// NVS Backend
// ==============================================================================

class NvsBackendTest : public ::testing::Test
{
protected:
    NiceMock<idf_hals::MockNvsHAL> nvs_hal;
    NvsBackend nvs{nvs_hal, "nvs_peers"};

    void SetUp() override
    {
        // happy path as default
        ON_CALL(nvs_hal, flash_init()).WillByDefault(Return(ESP_OK));
        ON_CALL(nvs_hal, open(_, _, _)).WillByDefault(Return(ESP_OK));
        ON_CALL(nvs_hal, close(_)).WillByDefault(Return());
        ON_CALL(nvs_hal, get_blob(_, _, _, _)).WillByDefault(Return(ESP_OK));
        ON_CALL(nvs_hal, set_blob(_, _, _, _)).WillByDefault(Return(ESP_OK));
        ON_CALL(nvs_hal, commit(_)).WillByDefault(Return(ESP_OK));
    }
};

// Test nvs_init
TEST_F(NvsBackendTest, NvsFlahsInitFailPropagatesError)
{
    EXPECT_CALL(nvs_hal, flash_init()).WillOnce(Return(ESP_FAIL)); // NVS init fails

    CoreStorage data = {};
    EXPECT_EQ(ESP_FAIL, nvs.save(&data, sizeof(CoreStorage))); // Returns error
}

TEST_F(NvsBackendTest, NvsFlahsInitWithMoFreePagesCallsEraseandInit)
{
    EXPECT_CALL(nvs_hal, flash_init())
        .Times(2)
        .WillOnce(Return(ESP_ERR_NVS_NO_FREE_PAGES)) // First call returns ESP_ERR_NVS_NO_FREE_PAGES
        .WillOnce(Return(ESP_OK));                   // Second call returns ESP_OK
    EXPECT_CALL(nvs_hal, flash_erase()).Times(1);    // Must call erase

    CoreStorage data = {};
    EXPECT_EQ(ESP_OK, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, NvsFlahsInitWithNewVersionCallsEraseandInit)
{
    EXPECT_CALL(nvs_hal, flash_init())
        .Times(2)
        .WillOnce(Return(ESP_ERR_NVS_NEW_VERSION_FOUND)) // First call returns ESP_ERR_NVS_NEW_VERSION_FOUND
        .WillOnce(Return(ESP_OK));                       // Second call returns ESP_OK
    EXPECT_CALL(nvs_hal, flash_erase()).Times(1);        // Must call erase

    CoreStorage data = {};
    EXPECT_EQ(ESP_OK, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, NvsFlahsInitFailsTwoTimesPropagatesError)
{
    EXPECT_CALL(nvs_hal, flash_init())
        .Times(2)
        .WillOnce(Return(ESP_ERR_NVS_NEW_VERSION_FOUND)) // First call returns ESP_ERR_NVS_NEW_VERSION_FOUND
        .WillOnce(Return(ESP_FAIL));                     // Second call returns ESP_FAIL
    EXPECT_CALL(nvs_hal, flash_erase()).Times(1);        // Must call erase

    CoreStorage data = {};
    EXPECT_EQ(ESP_FAIL, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SecondSaveSkipsFlashInit)
{
    // Firts save - nvs_initialized_ = false
    EXPECT_CALL(nvs_hal, flash_init()).Times(1); // Must call flash_init
    CoreStorage data = {};
    nvs.save(&data, sizeof(CoreStorage));

    // Second save - nvs_initialized_ = true
    EXPECT_CALL(nvs_hal, flash_init()).Times(0); // Must not call flash_init
    nvs.save(&data, sizeof(CoreStorage));
}

// Test NVS Save
TEST_F(NvsBackendTest, SaveFailsToOpenNvsPropagatesError)
{
    ON_CALL(nvs_hal, open(_, _, _)).WillByDefault(Return(ESP_FAIL)); // NVS open fails
    EXPECT_CALL(nvs_hal, set_blob(_, _, _, _)).Times(0);             // Must not call set_blob

    CoreStorage data = {};
    EXPECT_EQ(ESP_FAIL, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SaveFailsToSetBlobPropagatesError)
{
    ON_CALL(nvs_hal, set_blob(_, _, _, _)).WillByDefault(Return(ESP_FAIL)); // Returns ESP_FAIL
    EXPECT_CALL(nvs_hal, commit(_)).Times(0);                               // Must not call commit

    CoreStorage data = {};
    EXPECT_EQ(ESP_FAIL, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SaveFailsToCommitPropagatesError)
{
    ON_CALL(nvs_hal, commit(_)).WillByDefault(Return(ESP_FAIL)); // Returns ESP_FAIL

    CoreStorage data = {};
    EXPECT_EQ(ESP_FAIL, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SaveReturnsSuccess)
{
    EXPECT_CALL(nvs_hal, open(_, _, _)).Times(1);
    EXPECT_CALL(nvs_hal, set_blob(_, _, _, _)).Times(1);
    EXPECT_CALL(nvs_hal, commit(_)).Times(1);
    EXPECT_CALL(nvs_hal, close(_)).Times(1);

    CoreStorage data = {};
    EXPECT_EQ(ESP_OK, nvs.save(&data, sizeof(CoreStorage)));
}

// Load Tests
TEST_F(NvsBackendTest, LoadReturnsSuccess)
{
    CoreStorage data = {};
    EXPECT_EQ(ESP_OK, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadReturnsErroWhenNvsInitFails)
{
    ON_CALL(nvs_hal, flash_init()).WillByDefault(Return(ESP_FAIL)); // NVS init fails
    EXPECT_CALL(nvs_hal, open(_, _, _)).Times(0);                   // Must not call open

    CoreStorage data = {};
    EXPECT_EQ(ESP_FAIL, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadReturnsErroWhenNvsOpenFails)
{
    ON_CALL(nvs_hal, open(_, _, _)).WillByDefault(Return(ESP_FAIL)); // NVS open fails
    EXPECT_CALL(nvs_hal, get_blob(_, _, _, _)).Times(0);             // Must not call get_blob

    CoreStorage data = {};
    EXPECT_EQ(ESP_FAIL, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadReturnsErroWhenNvsGetBlobFails)
{
    ON_CALL(nvs_hal, get_blob(_, _, _, _)).WillByDefault(Return(ESP_FAIL)); // NVS get blob fails

    CoreStorage data = {}; // NVS get blob fails
    EXPECT_EQ(ESP_FAIL, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadReturnsErrorWhenSizeMismatch)
{
    size_t wrong_size = sizeof(CoreStorage) - 1;

    ON_CALL(nvs_hal, get_blob(_, _, _, _))
        .WillByDefault(DoAll(
            SetArgPointee<3>(wrong_size), // NVS get blob returns wrong size
            Return(ESP_OK)));             // NVS get blob returns ESP_OK

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_INVALID_SIZE, nvs.load(&data, sizeof(CoreStorage)));
}

// ==============================================================================
// RTC Backend
// ==============================================================================

// Although RTC_DATA_ATTR is not critical in test environment, it is used for consistency with production code
static RTC_DATA_ATTR CoreStorage g_rtc_storage;

class RtcBackendTest : public ::testing::Test
{
protected:
    // For testing purposes, we can use local storage instead of RTC
    // CoreStorage storage = {};  // local storage for RTC
    // RtcBackend backend{&storage}; // storage injection

    RtcBackend backend{&g_rtc_storage, sizeof(g_rtc_storage)}; // storage injection
};

TEST_F(RtcBackendTest, SaveAndLoadRoundtrip)
{
    CoreStorage original = {};
    original.magic = CoreStorage::CORE_MAGIC;
    original.version = CoreStorage::CORE_VERSION;
    original.boot_count = 2;

    EXPECT_EQ(ESP_OK, backend.save(&original, sizeof(CoreStorage)));

    CoreStorage loaded = {};
    EXPECT_EQ(ESP_OK, backend.load(&loaded, sizeof(CoreStorage)));

    EXPECT_EQ(loaded.magic, original.magic);
    EXPECT_EQ(loaded.boot_count, original.boot_count);
}

TEST_F(RtcBackendTest, LoadReturnsSizeErrorWhenTooLarge)
{
    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_INVALID_SIZE, backend.load(&data, sizeof(CoreStorage) + 1));
}
