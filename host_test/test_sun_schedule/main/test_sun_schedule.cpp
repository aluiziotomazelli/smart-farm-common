// components/smart-farm-common/host_test/test_sun_schedule/main/test_sun_schedule.cpp
#include <gtest/gtest.h>
#include <ctime>

#include "sun_schedule.hpp"

// Reference coordinates: São Paulo / Campinas (~ -23.0° Latitude, UTC-3)
static constexpr float LAT_SP = -23.0f;
static constexpr float TZ_SP = -3.0f;

// Helper to construct local epoch: year (e.g. 2026), month (1-12), day (1-31), hour (0-23), min (0-59)
static time_t make_epoch(int year, int month, int day, int hour, int min, float tz_offset = TZ_SP)
{
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = 0;
    t.tm_isdst = 0;
    
    // timegm converts UTC tm to epoch
    time_t utc_epoch = timegm(&t);
    // Adjust for the desired local timezone offset: local_time = utc + tz_offset => utc = local_time - tz_offset
    return utc_epoch - static_cast<time_t>(tz_offset * 3600.0f);
}

TEST(SunScheduleTest, EquinoxCalculation)
{
    SunSchedule sun(LAT_SP, TZ_SP);
    
    // March 21 (Equinox): day_of_year ~ 80. Day length should be approx 12.0 hours.
    time_t epoch = make_epoch(2026, 3, 21, 12, 0);
    SolarDayInfo info = sun.get_day_info(epoch);

    EXPECT_NEAR(info.day_length_hours, 12.0f, 0.2f);
    EXPECT_NEAR(info.sunrise_hour_local, 6.0f, 0.2f);
    EXPECT_NEAR(info.sunset_hour_local, 18.0f, 0.2f);
}

TEST(SunScheduleTest, IsDaytimeDetection)
{
    SunSchedule sun(LAT_SP, TZ_SP);

    // March 21: Sunrise ~ 06:00, Sunset ~ 18:00
    time_t night_dawn = make_epoch(2026, 3, 21, 5, 0);    // 05:00 -> Night
    time_t morning = make_epoch(2026, 3, 21, 8, 0);       // 08:00 -> Day
    time_t noon = make_epoch(2026, 3, 21, 12, 0);          // 12:00 -> Day
    time_t afternoon = make_epoch(2026, 3, 21, 16, 0);     // 16:00 -> Day
    time_t night_dusk = make_epoch(2026, 3, 21, 19, 0);    // 19:00 -> Night

    EXPECT_FALSE(sun.is_daytime(night_dawn));
    EXPECT_TRUE(sun.is_daytime(morning));
    EXPECT_TRUE(sun.is_daytime(noon));
    EXPECT_TRUE(sun.is_daytime(afternoon));
    EXPECT_FALSE(sun.is_daytime(night_dusk));
}

TEST(SunScheduleTest, HoursAndMinutesUntilSunset)
{
    SunSchedule sun(LAT_SP, TZ_SP);

    // March 21: Sunset ~ 18:00
    time_t at_15_00 = make_epoch(2026, 3, 21, 15, 0); // 3h before sunset
    EXPECT_NEAR(sun.hours_until_sunset(at_15_00), 3.0f, 0.2f);
    EXPECT_NEAR(sun.minutes_until_sunset(at_15_00), 180, 15);

    // Past sunset -> Should return 0.0f and 0 minutes
    time_t at_19_00 = make_epoch(2026, 3, 21, 19, 0);
    EXPECT_FLOAT_EQ(sun.hours_until_sunset(at_19_00), 0.0f);
    EXPECT_EQ(sun.minutes_until_sunset(at_19_00), 0);
}

TEST(SunScheduleTest, HoursSinceSunrise)
{
    SunSchedule sun(LAT_SP, TZ_SP);

    // March 21: Sunrise ~ 06:00
    time_t at_09_00 = make_epoch(2026, 3, 21, 9, 0); // 3h after sunrise
    EXPECT_NEAR(sun.hours_since_sunrise(at_09_00), 3.0f, 0.2f);

    // Before sunrise -> 0.0f
    time_t at_04_00 = make_epoch(2026, 3, 21, 4, 0);
    EXPECT_FLOAT_EQ(sun.hours_since_sunrise(at_04_00), 0.0f);
}

TEST(SunScheduleTest, SunElevationFactor)
{
    SunSchedule sun(LAT_SP, TZ_SP);

    time_t night = make_epoch(2026, 3, 21, 2, 0);
    time_t noon = make_epoch(2026, 3, 21, 12, 0);
    time_t morning = make_epoch(2026, 3, 21, 9, 0); // halfway to noon

    EXPECT_FLOAT_EQ(sun.get_sun_elevation_factor(night), 0.0f);
    EXPECT_NEAR(sun.get_sun_elevation_factor(noon), 1.0f, 0.05f);
    EXPECT_GT(sun.get_sun_elevation_factor(morning), 0.5f);
    EXPECT_LT(sun.get_sun_elevation_factor(morning), 1.0f);
}

TEST(SunScheduleTest, TwilightDetection)
{
    SunSchedule sun(LAT_SP, TZ_SP);

    // Sunset is ~ 18:00
    time_t at_17_45 = make_epoch(2026, 3, 21, 17, 45); // 15 min before sunset -> Twilight (margin 30 min)
    time_t at_18_15 = make_epoch(2026, 3, 21, 18, 15); // 15 min after sunset -> Twilight (margin 30 min)
    time_t at_12_00 = make_epoch(2026, 3, 21, 12, 0);  // Noon -> Not twilight
    time_t at_22_00 = make_epoch(2026, 3, 21, 22, 0);  // Night -> Not twilight

    EXPECT_TRUE(sun.is_twilight(at_17_45, 30));
    EXPECT_TRUE(sun.is_twilight(at_18_15, 30));
    EXPECT_FALSE(sun.is_twilight(at_12_00, 30));
    EXPECT_FALSE(sun.is_twilight(at_22_00, 30));
}
