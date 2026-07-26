#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "nvs_core.hpp"
#include "mock_hal_nvs.hpp"
#include "esp_rom_crc.h"
#include <cstddef>

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

// Concrete implementation for testing
class TestNvsCore : public NvsCore
{
public:
    TestNvsCore(idf_hals::INvsHAL& hal)
        : NvsCore("test_ns", hal)
    {
    }

    struct AppData
    {
        int value;
    } app_data;

    esp_err_t load_app_data() override { return load_struct("app_data", app_data); }

    esp_err_t save_app_data(bool force_nvs = false) override { return save_struct("app_data", app_data); }

    void set_app_defaults() override { app_data.value = 42; }
};

class NvsCoreTest : public ::testing::Test
{
protected:
    idf_hals::MockNvsHAL mock_hal;
    TestNvsCore nvs;

    NvsCoreTest()
        : nvs(mock_hal)
    {
    }

    void SetUp() override
    {
        nvs.invalidate_rtc_cache();
    }
};

TEST_F(NvsCoreTest, InitPartitionSuccess)
{
    EXPECT_CALL(mock_hal, flash_init()).WillOnce(Return(ESP_OK));

    EXPECT_EQ(nvs.init_partition(), ESP_OK);
}

TEST_F(NvsCoreTest, SaveForceNvsSuccess)
{
    nvs_handle_t fake_handle = 123;

    // Expect open
    EXPECT_CALL(mock_hal, open(testing::StrEq("test_ns"), NVS_READWRITE, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));

    // Expect save core_data
    EXPECT_CALL(mock_hal, set_blob(fake_handle, testing::StrEq("core_data"), _, _)).WillOnce(Return(ESP_OK));

    // Expect save app_data
    EXPECT_CALL(mock_hal, set_blob(fake_handle, testing::StrEq("app_data"), _, _)).WillOnce(Return(ESP_OK));

    // Expect commit
    EXPECT_CALL(mock_hal, commit(fake_handle)).WillOnce(Return(ESP_OK));

    // Expect close
    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.save(true), ESP_OK);
}

TEST_F(NvsCoreTest, InitPartitionErrorCallsEraseAndInit)
{
    testing::InSequence s;

    EXPECT_CALL(mock_hal, flash_init()).WillOnce(Return(ESP_ERR_NVS_NO_FREE_PAGES));
    EXPECT_CALL(mock_hal, flash_erase()).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, flash_init()).WillOnce(Return(ESP_OK));

    EXPECT_EQ(nvs.init_partition(), ESP_OK);
}

TEST_F(NvsCoreTest, OpenNvsFailReturnsError)
{
    EXPECT_CALL(mock_hal, open(testing::StrEq("test_ns"), NVS_READONLY, _))
        .WillOnce(Return(ESP_ERR_NVS_NOT_FOUND));
    EXPECT_EQ(nvs.load(), ESP_ERR_NVS_NOT_FOUND);
}

TEST_F(NvsCoreTest, LoadWithSchemaMismatchMigrates)
{
    nvs_handle_t fake_handle = 123;
    CoreStorage old_data = {};
    old_data.magic = CORE_STORAGE_MAGIC;
    old_data.schema_version = 0; // Old version
    old_data.crc = esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&old_data), offsetof(CoreStorage, crc));

    EXPECT_CALL(mock_hal, open(_, NVS_READONLY, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));

    // Return data with old version
    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("core_data"), _, _))
        .WillOnce(Invoke([old_data](nvs_handle_t, const char*, void* out, size_t* len) {
            if (out)
                memcpy(out, &old_data, sizeof(old_data));
            if (len)
                *len = sizeof(old_data);
            return ESP_OK;
        }));

    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("app_data"), _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.load(), ESP_OK);
    EXPECT_EQ(nvs.get_core_data().schema_version, CORE_SCHEMA_VERSION);
}

TEST_F(NvsCoreTest, LoadWithAppDataFailReturnsError)
{
    nvs_handle_t fake_handle = 123;
    CoreStorage valid_core = {};
    valid_core.magic = CORE_STORAGE_MAGIC;
    valid_core.crc = esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&valid_core), offsetof(CoreStorage, crc));

    EXPECT_CALL(mock_hal, open(_, NVS_READONLY, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));

    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("core_data"), _, _))
        .WillOnce(Invoke([valid_core](nvs_handle_t, const char*, void* out, size_t* len) {
            if (out)
                memcpy(out, &valid_core, sizeof(valid_core));
            if (len)
                *len = sizeof(valid_core);
            return ESP_OK;
        }));

    // Simulate app data load failure
    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("app_data"), _, _))
        .WillOnce(Return(ESP_ERR_NVS_NOT_FOUND));

    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.load(), ESP_ERR_NVS_NOT_FOUND);
}

TEST_F(NvsCoreTest, EraseNamespaceSuccess)
{
    nvs_handle_t fake_handle = 123;
    testing::InSequence s;

    EXPECT_CALL(mock_hal, open(testing::StrEq("test_ns"), NVS_READWRITE, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));

    EXPECT_CALL(mock_hal, erase_all(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, commit(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.erase_namespace(), ESP_OK);
}

TEST_F(NvsCoreTest, FactoryResetSequence)
{
    nvs_handle_t fake_handle = 123;
    testing::InSequence s;

    // 1. Erase Namespace
    EXPECT_CALL(mock_hal, open(testing::StrEq("test_ns"), NVS_READWRITE, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));
    EXPECT_CALL(mock_hal, erase_all(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, commit(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    // 2. Commit Defaults (factory_reset calls apply_core_defaults, setAppDefaults then save(true))
    EXPECT_CALL(mock_hal, open(testing::StrEq("test_ns"), NVS_READWRITE, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));
    EXPECT_CALL(mock_hal, set_blob(fake_handle, testing::StrEq("core_data"), _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, set_blob(fake_handle, testing::StrEq("app_data"), _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, commit(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    nvs.factory_reset();

    // Verify that data is reset in memory
    EXPECT_EQ(nvs.get_core_data().schema_version, CORE_SCHEMA_VERSION);
    EXPECT_EQ(nvs.app_data.value, 42); // Value from setAppDefaults
}

TEST_F(NvsCoreTest, LoadFromRtcSuccess)
{
    nvs_handle_t fake_handle = 123;

    // First save to populate RTC memory
    EXPECT_CALL(mock_hal, open(_, NVS_READWRITE, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));
    EXPECT_CALL(mock_hal, set_blob(_, _, _, _)).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(mock_hal, commit(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    nvs.save(true);

    // Now call load() -> core_ is loaded from RTC without reading core_data from NVS!
    EXPECT_CALL(mock_hal, open(_, NVS_READONLY, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));
    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("app_data"), _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.load(), ESP_OK);
}

TEST_F(NvsCoreTest, LoadWithInvalidNvsMagicFallsBackToDefaults)
{
    nvs_handle_t fake_handle = 123;
    CoreStorage invalid_magic_core = {};
    invalid_magic_core.magic = 0xDEADBEEF; // Bad magic
    invalid_magic_core.crc = esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&invalid_magic_core), offsetof(CoreStorage, crc));

    EXPECT_CALL(mock_hal, open(_, NVS_READONLY, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));

    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("core_data"), _, _))
        .WillOnce(Invoke([invalid_magic_core](nvs_handle_t, const char*, void* out, size_t* len) {
            if (out) memcpy(out, &invalid_magic_core, sizeof(invalid_magic_core));
            if (len) *len = sizeof(invalid_magic_core);
            return ESP_OK;
        }));

    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("app_data"), _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.load(), ESP_OK);
    // Core should fall back to default values with valid magic
    EXPECT_EQ(nvs.get_core_data().magic, CORE_STORAGE_MAGIC);
}

TEST_F(NvsCoreTest, LoadWithInvalidNvsCrcFallsBackToDefaults)
{
    nvs_handle_t fake_handle = 123;
    CoreStorage invalid_crc_core = {};
    invalid_crc_core.magic = CORE_STORAGE_MAGIC;
    invalid_crc_core.crc = 0xBAD0F00D; // Corrupt CRC

    EXPECT_CALL(mock_hal, open(_, NVS_READONLY, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));

    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("core_data"), _, _))
        .WillOnce(Invoke([invalid_crc_core](nvs_handle_t, const char*, void* out, size_t* len) {
            if (out) memcpy(out, &invalid_crc_core, sizeof(invalid_crc_core));
            if (len) *len = sizeof(invalid_crc_core);
            return ESP_OK;
        }));

    EXPECT_CALL(mock_hal, get_blob(fake_handle, testing::StrEq("app_data"), _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.load(), ESP_OK);
    EXPECT_EQ(nvs.get_core_data().magic, CORE_STORAGE_MAGIC);
}

TEST_F(NvsCoreTest, SaveNotDirtyDoesNotWriteToNvs)
{
    nvs_handle_t fake_handle = 123;

    // 1. Initial save(true) to sync RTC
    EXPECT_CALL(mock_hal, open(_, NVS_READWRITE, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));
    EXPECT_CALL(mock_hal, set_blob(_, _, _, _)).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(mock_hal, commit(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    nvs.save(true);

    // 2. Call save(false) when data is clean -> core_data set_blob is NOT called! Only app_data is saved
    EXPECT_CALL(mock_hal, open(_, NVS_READWRITE, _))
        .WillOnce(DoAll(SetArgPointee<2>(fake_handle), Return(ESP_OK)));
    EXPECT_CALL(mock_hal, set_blob(fake_handle, testing::StrEq("app_data"), _, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, commit(fake_handle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(mock_hal, close(fake_handle));

    EXPECT_EQ(nvs.save(false), ESP_OK);
}
