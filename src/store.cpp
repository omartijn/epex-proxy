#include "store.hpp"
#include "calculator.hpp"
#include "time_util.hpp"
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json   = nlohmann::json;

const CachedDay* PriceStore::find_day(std::string_view area_id, std::string_view date) const {
    auto ait = areas.find(std::string(area_id));
    if (ait == areas.end()) {
        return nullptr;
    }
    auto dit = ait->second.days.find(std::string(date));
    if (dit == ait->second.days.end()) {
        return nullptr;
    }
    return &dit->second;
}

// Disk format: { "date": "YYYY-MM-DD", "slots": [{"start_utc": <epoch_s>, "epex_mwh": <f>}, ...] }
static DayPrices from_json(const json& j) {
    DayPrices dp{.date = j["date"].get<std::string>(), .slots = {}};
    dp.slots.reserve(96);
    for (const auto& s : j["slots"]) {
        dp.slots.push_back({
            .start    = std::chrono::system_clock::time_point{
                            std::chrono::seconds{s["start_utc"].get<int64_t>()}},
            .epex_mwh = s["epex_mwh"]
        });
    }
    return dp;
}

void load_from_disk(PriceStore& store, const AppConfig& cfg) {
    for (const auto& [area_id, area] : cfg.areas) {
        auto dir = fs::path(cfg.data_dir) / area_id;
        if (!fs::exists(dir)) {
            continue;
        }

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() != ".json") {
                continue;
            }
            std::ifstream f(entry.path());
            if (!f) {
                continue;
            }
            try {
                auto prices = from_json(json::parse(f));
                auto cached = make_cached_day(prices, area);
                store.areas[area_id].days[prices.date] = std::move(cached);
            } catch (const std::exception& e) {
                std::cerr << "Skipping corrupt cache file "
                          << entry.path() << ": " << e.what() << '\n';
            }
        }
    }
    regenerate_health(store, cfg);
}

void save_to_disk(const std::string& data_dir, std::string_view area_id, const DayPrices& prices) {
    auto dir = fs::path(data_dir) / area_id;
    fs::create_directories(dir);

    json j;
    j["date"]  = prices.date;
    j["slots"] = json::array();
    for (const auto& slot : prices.slots) {
        j["slots"].push_back({
            {"start_utc", std::chrono::duration_cast<std::chrono::seconds>(
                              slot.start.time_since_epoch()).count()},
            {"epex_mwh",  slot.epex_mwh}
        });
    }

    std::ofstream f(fs::path(dir) / (prices.date + ".json"));
    f << j.dump(2);
}

void regenerate_health(PriceStore& store, const AppConfig& cfg) {
    auto fmt_tp = [](std::chrono::system_clock::time_point tp) {
        return std::format("{:%Y-%m-%dT%H:%M:%SZ}",
            std::chrono::floor<std::chrono::seconds>(tp));
    };

    json j;
    j["status"] = "ok";
    j["areas"]  = json::object();

    for (const auto& [area_id, area] : cfg.areas) {
        auto ait           = store.areas.find(area_id);
        std::string today  = local_date(area);
        std::string tmr    = local_date_tomorrow(area);

        json area_j;

        // Today
        const auto* today_day = store.find_day(area_id, today);
        if (today_day) {
            area_j["today"] = {{"date", today}, {"slots", today_day->prices.slots.size()}};
        } else {
            j["status"]     = "degraded";
            area_j["today"] = {{"date", today}, {"status", "missing"}};
        }

        // Tomorrow
        json tmr_j;
        tmr_j["date"] = tmr;
        if (ait != store.areas.end()) {
            const auto& fs = ait->second.tomorrow_fetch;
            switch (fs.status) {
            case FetchStatus::ok: {
                tmr_j["status"] = "ok";
                const auto* d = store.find_day(area_id, tmr);
                if (d) {
                    tmr_j["slots"] = d->prices.slots.size();
                }
                break;
            }
            case FetchStatus::pending:
                tmr_j["status"]   = "pending";
                tmr_j["attempts"] = fs.attempts;
                if (fs.last_attempt) {
                    tmr_j["last_attempt"] = fmt_tp(*fs.last_attempt);
                }
                if (fs.last_http_status) {
                    tmr_j["last_http_status"] = *fs.last_http_status;
                }
                if (fs.last_error_body) {
                    tmr_j["last_error"] = *fs.last_error_body;
                }
                break;
            case FetchStatus::not_started:
                tmr_j["status"] = "unavailable";
                break;
            }
        } else {
            tmr_j["status"] = "unavailable";
        }
        area_j["tomorrow"] = std::move(tmr_j);

        j["areas"][area_id] = std::move(area_j);
    }

    store.health_json = j.dump(2);
}
