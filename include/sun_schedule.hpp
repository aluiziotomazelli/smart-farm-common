// components/smart-farm-common/include/sun_schedule.hpp
#pragma once

#include <cmath>
#include <cstdint>
#include <ctime>

/**
 * @struct SolarDayInfo
 * @brief Astronomical sun calculation results for a given day of year.
 */
struct SolarDayInfo
{
    float day_length_hours = 12.0f;   ///< Total day length in hours
    float sunrise_hour_local = 6.0f;  ///< Calculated local sunrise hour (0.0 - 24.0)
    float sunset_hour_local = 18.0f;  ///< Calculated local sunset hour (0.0 - 24.0)
};

/**
 * @class SunSchedule
 * @brief Pure domain calculator for solar positions, day/night schedules, and twilight transitions.
 */
class SunSchedule
{
public:
    static constexpr float DEFAULT_LATITUDE_DEG = -23.0f;
    static constexpr float DEFAULT_TZ_OFFSET_HOURS = -3.0f;

    /**
     * @brief Constructs SunSchedule with latitude and timezone offset.
     * @param latitude_deg Geographical latitude in degrees (South negative, North positive).
     * @param tz_offset_hours Local timezone offset from UTC in hours (e.g., -3.0 for BRT).
     */
    explicit SunSchedule(
        float latitude_deg = DEFAULT_LATITUDE_DEG,
        float tz_offset_hours = DEFAULT_TZ_OFFSET_HOURS);

    void set_location(float latitude_deg, float tz_offset_hours);

    /**
     * @brief Gets current geographical latitude in degrees.
     */
    float get_latitude_deg() const { return latitude_deg_; }

    /**
     * @brief Gets current local timezone offset in hours.
     */
    float get_tz_offset_hours() const { return tz_offset_hours_; }

    /**
     * @brief Calculates astronomical day parameters for a given Unix timestamp.
     */
    SolarDayInfo get_day_info(time_t epoch) const;

    /**
     * @brief Checks if the timestamp falls between sunrise and sunset.
     */
    bool is_daytime(time_t epoch) const;

    /**
     * @brief Returns decimal hours remaining until sunset today (0.0f if past sunset).
     */
    float hours_until_sunset(time_t epoch) const;

    /**
     * @brief Returns decimal hours elapsed since sunrise today (0.0f if before sunrise).
     */
    float hours_since_sunrise(time_t epoch) const;

    /**
     * @brief Returns integer minutes remaining until sunset today (0 if past sunset).
     */
    int32_t minutes_until_sunset(time_t epoch) const;

    /**
     * @brief Returns a normalized solar elevation factor from 0.0f (night) to 1.0f (solar noon).
     *
     * Useful for smooth LED dimming or time-of-day brightness curves.
     */
    float get_sun_elevation_factor(time_t epoch) const;

    /**
     * @brief Checks if timestamp is within the twilight margin before/after sunset or sunrise.
     */
    bool is_twilight(time_t epoch, uint16_t twilight_margin_minutes = 30) const;

    /**
     * @brief Static pure calculation using Cooper's equation.
     */
    static SolarDayInfo calculate_solar_day(uint16_t day_of_year, float latitude_deg);

private:
    struct LocalTime {
        float decimal_hour = 0.0f;
        uint16_t day_of_year = 1;
    };

    LocalTime decompose(time_t epoch) const;

    float latitude_deg_;
    float tz_offset_hours_;
};
