#include "time_util.hpp"
#include <chrono>
#include <format>

static const std::chrono::time_zone* get_tz(const AreaConfig& area) {
    return std::chrono::locate_zone(area.timezone);
}

std::string local_date(const AreaConfig& area) {
    auto zt = std::chrono::zoned_time{get_tz(area), std::chrono::system_clock::now()};
    auto ld = std::chrono::floor<std::chrono::days>(zt.get_local_time());
    return std::format("{:%Y-%m-%d}", std::chrono::year_month_day{ld});
}

std::string local_date_tomorrow(const AreaConfig& area) {
    auto zt = std::chrono::zoned_time{get_tz(area), std::chrono::system_clock::now()};
    auto ld = std::chrono::floor<std::chrono::days>(zt.get_local_time()) + std::chrono::days{1};
    return std::format("{:%Y-%m-%d}", std::chrono::year_month_day{ld});
}

std::chrono::system_clock::time_point next_local_midnight(const AreaConfig& area) {
    const auto* tz = get_tz(area);
    auto zt      = std::chrono::zoned_time{tz, std::chrono::system_clock::now()};
    auto ld      = std::chrono::floor<std::chrono::days>(zt.get_local_time());
    auto midnight = std::chrono::local_seconds{ld + std::chrono::days{1}};
    // choose::earliest handles the ambiguous-local-time edge case at DST end
    return std::chrono::zoned_time{tz, midnight, std::chrono::choose::earliest}.get_sys_time();
}

std::string entsoe_period_utc(const AreaConfig& area, std::string_view date_ymd) {
    using namespace std::chrono;
    const auto* tz = get_tz(area);
    int      y = std::stoi(std::string(date_ymd.substr(0, 4)));
    unsigned m = static_cast<unsigned>(std::stoi(std::string(date_ymd.substr(5, 2))));
    unsigned d = static_cast<unsigned>(std::stoi(std::string(date_ymd.substr(8, 2))));
    auto local_midnight = local_seconds{
        local_days{year_month_day{year{y}, month{m}, day{d}}}
    };
    auto utc = zoned_time{tz, local_midnight, choose::earliest}.get_sys_time();
    return std::format("{:%Y%m%d%H%M}", utc);
}

std::chrono::system_clock::time_point next_fetch_time(const AreaConfig& area) {
    const auto* tz = get_tz(area);
    auto now   = std::chrono::system_clock::now();
    auto zt    = std::chrono::zoned_time{tz, now};
    auto today = std::chrono::floor<std::chrono::days>(zt.get_local_time());

    auto t1300 = std::chrono::zoned_time{
        tz, std::chrono::local_seconds{today} + std::chrono::hours{13}
    }.get_sys_time();

    if (now < t1300) {
        return t1300;
    }

    return std::chrono::zoned_time{
        tz, std::chrono::local_seconds{today + std::chrono::days{1}} + std::chrono::hours{13}
    }.get_sys_time();
}
