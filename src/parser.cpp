#include "parser.hpp"
#include <algorithm>
#include <chrono>
#include <pugixml.hpp>
#include <stdexcept>
#include <string>

// Parse "2026-05-17T22:00Z" → system_clock::time_point (UTC)
static std::chrono::system_clock::time_point parse_timestamp(std::string_view s) {
    // Expected format: YYYY-MM-DDTHH:MMZ  (ENTSO-E omits seconds)
    if (s.size() < 17 || s.back() != 'Z') {
        throw std::runtime_error("Unexpected ENTSO-E timestamp: " + std::string(s));
    }

    auto tok = [&](std::size_t off, std::size_t len) {
        return std::stoi(std::string(s.substr(off, len)));
    };
    int year   = tok(0,  4);
    int month  = tok(5,  2);
    int day    = tok(8,  2);
    int hour   = tok(11, 2);
    int minute = tok(14, 2);

    using namespace std::chrono;
    auto ymd = year_month_day{
        std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)}
    };
    return sys_days{ymd} + hours{hour} + minutes{minute};
}

DayPrices parse_entsoe_xml(std::string_view xml, std::string_view date_local) {
    pugi::xml_document doc;
    auto result = doc.load_buffer(xml.data(), xml.size());
    if (!result) {
        throw std::runtime_error(std::string("XML parse failed: ") + result.description());
    }

    DayPrices dp{.date = std::string(date_local), .slots = {}};

    // The ENTSO-E document root may have a default namespace; pugixml does not
    // strip namespaces, so we search by local name via a child walk.
    auto root = doc.first_child();  // Publication_MarketDocument

    for (auto ts : root.children("TimeSeries")) {
        for (auto period : ts.children("Period")) {
            if (std::string_view{period.child_value("resolution")} != "PT15M") {
                continue;
            }

            auto interval    = period.child("timeInterval");
            auto period_start = parse_timestamp(interval.child_value("start"));
            auto period_end   = parse_timestamp(interval.child_value("end"));

            constexpr auto slot_dur = std::chrono::minutes{15};
            auto total_slots = (period_end - period_start) / slot_dur;

            // ENTSO-E publishes a sparse curve: a Point whose price equals the
            // previous position's is omitted, and the value is carried forward
            // until the next given position (and to the end of the period). So
            // we backfill skipped positions with the last seen price rather
            // than leaving gaps. total_slots comes from the timeInterval, so
            // short (DST spring) and long (DST autumn) days fill correctly too.
            auto emit = [&](long pos, double price) {
                dp.slots.push_back({
                    .start    = period_start + slot_dur * (pos - 1),
                    .epex_mwh = price,
                });
            };

            long   prev_pos   = 0;
            double prev_price = 0.0;
            for (auto point : period.children("Point")) {
                long   pos   = std::stol(point.child_value("position"));
                double price = std::stod(point.child_value("price.amount"));

                for (long p = prev_pos + 1; p < pos; ++p) {  // backfill gaps
                    emit(p, prev_price);
                }
                emit(pos, price);

                prev_pos   = pos;
                prev_price = price;
            }

            // Carry the final price forward to the end of the period.
            for (long p = prev_pos + 1; p <= total_slots; ++p) {
                emit(p, prev_price);
            }
        }
    }

    if (dp.slots.empty()) {
        throw std::runtime_error("No PT15M TimeSeries found in ENTSO-E response");
    }

    std::sort(dp.slots.begin(), dp.slots.end(),
        [](const PriceSlot& a, const PriceSlot& b) { return a.start < b.start; });

    return dp;
}
