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
    EXPECT_CALL(nvs_hal, flash_init()).WillRepeatedly(Return(ESP_ERR_NVS_NO_FREE_PAGES));
    EXPECT_CALL(nvs_hal, flash_erase()).Times(1);

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_NVS_NO_FREE_PAGES, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, NvsOpenFailPropagatesError)
{
    EXPECT_CALL(nvs_hal, open(_, _, _)).WillOnce(Return(ESP_ERR_NVS_NOT_FOUND));

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_NVS_NOT_FOUND, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, NvsInitOnce)
{
    EXPECT_CALL(nvs_hal, flash_init()).Times(1);
    EXPECT_CALL(nvs_hal, open(_, _, _)).Times(2);

    CoreStorage data = {};
    EXPECT_EQ(ESP_OK, nvs.load(&data, sizeof(CoreStorage)));
    EXPECT_EQ(ESP_OK, nvs.load(&data, sizeof(CoreStorage)));
}

// Test save
TEST_F(NvsBackendTest, SaveSuccess)
{
    EXPECT_CALL(nvs_hal, set_blob(_, _, _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(nvs_hal, commit(_)).WillOnce(Return(ESP_OK));

    CoreStorage data = {};
    EXPECT_EQ(ESP_OK, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SaveSetBlobFailPropagatesError)
{
    EXPECT_CALL(nvs_hal, set_blob(_, _, _, _)).WillOnce(Return(ESP_ERR_NVS_NOT_ENOUGH_SPACE));

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_NVS_NOT_ENOUGH_SPACE, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SaveCommitFailPropagatesError)
{
    EXPECT_CALL(nvs_hal, commit(_)).WillOnce(Return(ESP_ERR_NVS_READ_ONLY));

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_NVS_READ_ONLY, nvs.save(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SaveDataNullReturnsInvalidArg)
{
    EXPECT_EQ(ESP_ERR_INVALID_ARG, nvs.save(nullptr, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, SaveSizeZeroReturnsInvalidArg)
{
    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_INVALID_ARG, nvs.save(&data, 0));
}

// Test load
TEST_F(NvsBackendTest, LoadSuccess)
{
    EXPECT_CALL(nvs_hal, get_blob(_, _, _, _)).WillOnce(Return(ESP_OK));

    CoreStorage data = {};
    EXPECT_EQ(ESP_OK, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadGetBlobFailPropagatesError)
{
    EXPECT_CALL(nvs_hal, get_blob(_, _, _, _)).WillOnce(Return(ESP_ERR_NVS_NOT_FOUND));

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_NVS_NOT_FOUND, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadDataNullReturnsInvalidArg)
{
    EXPECT_EQ(ESP_ERR_INVALID_ARG, nvs.load(nullptr, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadSizeZeroReturnsInvalidArg)
{
    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_INVALID_ARG, nvs.load(&data, 0));
}

TEST_F(NvsBackendTest, LoadHandlesNvsNotFoundReturnsNotFound)
{
    EXPECT_CALL(nvs_hal, get_blob(_, _, _, _)).WillOnce(Return(ESP_ERR_NVS_NOT_FOUND));

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_NVS_NOT_FOUND, nvs.load(&data, sizeof(CoreStorage)));
}

TEST_F(NvsBackendTest, LoadReturnsInvalidSizeWhenBlobSizeDoesNotMatch)
{
    size_t wrong_size = sizeof(CoreStorage) - 1;

    ON_CALL(nvs_hal, get_blob(_, _, _, _))
        .WillByDefault(DoAll(
            SetArgPointee<3>(wrong_size),
            Return(ESP_OK)));

    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_INVALID_SIZE, nvs.load(&data, sizeof(CoreStorage)));
}

// ==============================================================================
// RTC Backend
// ==============================================================================

static RTC_DATA_ATTR CoreStorage g_rtc_storage;

class RtcBackendTest : public ::testing::Test
{
protected:
    RtcBackend backend{&g_rtc_storage, sizeof(g_rtc_storage)};
};

TEST_F(RtcBackendTest, SaveAndLoadRoundtrip)
{
    CoreStorage original = {};
    original.magic = CORE_MAGIC;
    original.version = CORE_VERSION;
    original.data.boot_count = 2;

    EXPECT_EQ(ESP_OK, backend.save(&original, sizeof(CoreStorage)));

    CoreStorage loaded = {};
    EXPECT_EQ(ESP_OK, backend.load(&loaded, sizeof(CoreStorage)));

    EXPECT_EQ(loaded.magic, original.magic);
    EXPECT_EQ(loaded.data.boot_count, original.data.boot_count);
}

TEST_F(RtcBackendTest, LoadReturnsSizeErrorWhenTooLarge)
{
    CoreStorage data = {};
    EXPECT_EQ(ESP_ERR_INVALID_SIZE, backend.load(&data, sizeof(CoreStorage) + 1));
}
