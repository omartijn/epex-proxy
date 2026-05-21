#pragma once
#include "config.hpp"
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct PriceSlot {
    std::chrono::system_clock::time_point start;  // UTC
    double epex_mwh;
};

struct DayPrices {
    std::string date;              // YYYY-MM-DD in area-local timezone
    std::vector<PriceSlot> slots;  // 96 entries for PT15M
};

struct CachedDay {
    DayPrices prices;
    std::string raw_json;
    std::unordered_map<std::string, std::string> provider_json;         // provider_id -> JSON incl VAT
    std::unordered_map<std::string, std::string> provider_json_ex_vat;  // provider_id -> JSON excl VAT
};

enum class FetchStatus : uint8_t { not_started, pending, ok };

struct FetchState {
    FetchStatus status{FetchStatus::not_started};
    int         attempts{0};
    std::optional<std::chrono::system_clock::time_point> last_attempt;
    std::optional<int>         last_http_status;
    std::optional<std::string> last_error_body;  // truncated to 512 chars
};

struct AreaStore {
    std::map<std::string, CachedDay> days;  // YYYY-MM-DD -> cached day
    FetchState today_fetch;
    FetchState tomorrow_fetch;
};

struct PriceStore {
    std::unordered_map<std::string, AreaStore> areas;
    std::string health_json;  // pre-generated; rebuilt on every fetch/midnight event
    bool shutting_down{false};

    const CachedDay* find_day(std::string_view area_id, std::string_view date) const;
};

// Load all day files from disk on startup; regenerates CachedDay via calculator
void load_from_disk(PriceStore& store, const AppConfig& cfg);

// Persist raw prices for one day (called after every successful fetch)
void save_to_disk(const std::string& data_dir, std::string_view area_id, const DayPrices& prices);

// Rebuild store.health_json from current state; call after any fetch or midnight event
void regenerate_health(PriceStore& store, const AppConfig& cfg);
