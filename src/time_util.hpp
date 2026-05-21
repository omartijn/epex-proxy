#pragma once
#include "config.hpp"
#include <chrono>
#include <string>

// Current local date (YYYY-MM-DD) for the area's timezone
std::string local_date(const AreaConfig& area);

// Tomorrow's local date (YYYY-MM-DD) for the area's timezone
std::string local_date_tomorrow(const AreaConfig& area);

// UTC time_point of the next local midnight in the area's timezone
std::chrono::system_clock::time_point next_local_midnight(const AreaConfig& area);

// UTC time_point of the next 13:00 local fetch window:
// returns today's 13:00 if we haven't passed it yet, otherwise tomorrow's.
std::chrono::system_clock::time_point next_fetch_time(const AreaConfig& area);

// ENTSO-E period string (YYYYMMDDHHMI, UTC) for local midnight of date_ymd (YYYY-MM-DD).
std::string entsoe_period_utc(const AreaConfig& area, std::string_view date_ymd);
