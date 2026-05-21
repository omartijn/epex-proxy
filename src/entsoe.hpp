#pragma once
#include <boost/asio/awaitable.hpp>
#include <string>
#include <string_view>

struct EntsoeFetchResult {
    int         http_status{};
    std::string body;
};

// Fetch the ENTSO-E A44 day-ahead XML for the given bidding zone and period.
// period_start / period_end format: "YYYYMMDD0000" (UTC midnight).
// Throws boost::system::system_error on network/TLS failure.
boost::asio::awaitable<EntsoeFetchResult> fetch_entsoe(
    std::string_view token,
    std::string_view bidding_zone,
    std::string_view period_start,
    std::string_view period_end
);
