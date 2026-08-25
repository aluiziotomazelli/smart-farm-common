// components/smart-farm-common/src/sun_schedule.cpp
#include <algorithm>
#include <cmath>

#include "sun_schedule.hpp"

SunSchedule::SunSchedule(float latitude_deg, float tz_offset_hours)
    : latitude_deg_(latitude_deg)
    , tz_offset_hours_(tz_offset_hours)
{
}

void SunSchedule::set_location(float latitude_deg, float tz_offset_hours)
{
    latitude_deg_ = latitude_deg;
    tz_offset_hours_ = tz_offset_hours;
}

SolarDayInfo SunSchedule::calculate_solar_day(uint16_t day_of_year, float latitude_deg)
{
    constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / 3.14159265358979323846f;

    // Solar declination (Cooper's equation)
    float declination_deg = 23.45f * std::sin((360.0f / 365.0f) * static_cast<float>(day_of_year - 81) * DEG_TO_RAD);

    float lat_rad = latitude_deg * DEG_TO_RAD;
    float dec_rad = declination_deg * DEG_TO_RAD;

    float cos_h0 = -std::tan(lat_rad) * std::tan(dec_rad);

    SolarDayInfo info;
    if (cos_h0 >= 1.0f) {
        info.day_length_hours = 0.0f;
        info.sunrise_hour_local = 12.0f;
        info.sunset_hour_local = 12.0f;
        return info;
    }
    if (cos_h0 <= -1.0f) {
        info.day_length_hours = 24.0f;
        info.sunrise_hour_local = 0.0f;
        info.sunset_hour_local = 24.0f;
        return info;
    }

    float h0_deg = std::acos(cos_h0) * RAD_TO_DEG;
    info.day_length_hours = (2.0f * h0_deg) / 15.0f;

    float solar_noon = 12.0f;
    info.sunrise_hour_local = solar_noon - (info.day_length_hours / 2.0f);
    info.sunset_hour_local = solar_noon + (info.day_length_hours / 2.0f);

    return info;
}

SunSchedule::LocalTime SunSchedule::decompose(time_t epoch) const
{
    int64_t offset_sec = static_cast<int64_t>(tz_offset_hours_ * 3600.0f);
    time_t local_unix = epoch + offset_sec;

    struct tm tm_info = {};
    gmtime_r(&local_unix, &tm_info);

    LocalTime lt;
    lt.decimal_hour = static_cast<float>(tm_info.tm_hour) +
                      (static_cast<float>(tm_info.tm_min) / 60.0f) +
                      (static_cast<float>(tm_info.tm_sec) / 3600.0f);
    lt.day_of_year = static_cast<uint16_t>(tm_info.tm_yday + 1);
    return lt;
}

SolarDayInfo SunSchedule::get_day_info(time_t epoch) const
{
    LocalTime lt = decompose(epoch);
    return calculate_solar_day(lt.day_of_year, latitude_deg_);
}

bool SunSchedule::is_daytime(time_t epoch) const
{
    LocalTime lt = decompose(epoch);
    SolarDayInfo day = calculate_solar_day(lt.day_of_year, latitude_deg_);
    return (lt.decimal_hour >= day.sunrise_hour_local && lt.decimal_hour < day.sunset_hour_local);
}

float SunSchedule::hours_until_sunset(time_t epoch) const
{
    LocalTime lt = decompose(epoch);
    SolarDayInfo day = calculate_solar_day(lt.day_of_year, latitude_deg_);

    if (lt.decimal_hour < day.sunrise_hour_local || lt.decimal_hour >= day.sunset_hour_local) {
        return 0.0f;
    }
    return day.sunset_hour_local - lt.decimal_hour;
}

float SunSchedule::hours_since_sunrise(time_t epoch) const
{
    LocalTime lt = decompose(epoch);
    SolarDayInfo day = calculate_solar_day(lt.day_of_year, latitude_deg_);

    if (lt.decimal_hour < day.sunrise_hour_local || lt.decimal_hour >= day.sunset_hour_local) {
        return 0.0f;
    }
    return lt.decimal_hour - day.sunrise_hour_local;
}

int32_t SunSchedule::minutes_until_sunset(time_t epoch) const
{
    float h = hours_until_sunset(epoch);
    return static_cast<int32_t>(std::round(h * 60.0f));
}

float SunSchedule::get_sun_elevation_factor(time_t epoch) const
{
    LocalTime lt = decompose(epoch);
    SolarDayInfo day = calculate_solar_day(lt.day_of_year, latitude_deg_);

    if (lt.decimal_hour < day.sunrise_hour_local || lt.decimal_hour >= day.sunset_hour_local) {
        return 0.0f;
    }

    float half_day = day.day_length_hours / 2.0f;

    if (half_day <= 0.001f) {
        return 0.0f;
    }

    // Sinusoidal arc from 0.0 to 1.0 at noon
    float progress = (lt.decimal_hour - day.sunrise_hour_local) / day.day_length_hours; // 0.0 to 1.0
    return std::sin(progress * 3.14159265358979323846f);
}

bool SunSchedule::is_twilight(time_t epoch, uint16_t twilight_margin_minutes) const
{
    LocalTime lt = decompose(epoch);
    SolarDayInfo day = calculate_solar_day(lt.day_of_year, latitude_deg_);
    float margin_hours = static_cast<float>(twilight_margin_minutes) / 60.0f;

    bool near_sunrise = std::abs(lt.decimal_hour - day.sunrise_hour_local) <= margin_hours;
    bool near_sunset = std::abs(lt.decimal_hour - day.sunset_hour_local) <= margin_hours;

    return (near_sunrise || near_sunset);
}
