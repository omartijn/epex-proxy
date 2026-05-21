#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// A single entry in a time-varying tariff schedule.
// valid_from is a "YYYY-MM-DD" date; pick the latest entry whose valid_from <= date.
struct TariffRate {
    std::string valid_from;
    double      value{};
};

// Returns the rate in effect on `date` ("YYYY-MM-DD"). Rates must be sorted ascending.
inline double lookup_rate(const std::vector<TariffRate>& rates, std::string_view date) {
    if (rates.empty()) {
        throw std::runtime_error("empty tariff rate schedule");
    }
    double result = rates.front().value;
    for (const auto& r : rates) {
        if (r.valid_from > date) {
            break;
        }
        result = r.value;
    }
    return result;
}

struct ProviderConfig {
    std::string id;
    std::string name;
    std::vector<TariffRate> delivery_fee;
    std::vector<TariffRate> redelivery_fee;
};

struct AreaConfig {
    std::string id;
    std::string name;
    std::string bidding_zone;
    std::string timezone;  // IANA tz name, e.g. "Europe/Amsterdam"
    std::vector<TariffRate> energy_tax;
    double vat_rate{};
    std::unordered_map<std::string, ProviderConfig> providers;
};

struct ListenEndpoint {
    std::string address;
    uint16_t    port{8080};
};

struct AppConfig {
    std::vector<ListenEndpoint> listen;
    std::string                 data_dir;
    std::unordered_map<std::string, AreaConfig> areas;
};

AppConfig load_config(const std::string& path);
