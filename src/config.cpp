#include "config.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

static std::vector<TariffRate> parse_rates(const nlohmann::json& j) {
    std::vector<TariffRate> rates;
    for (const auto& entry : j) {
        rates.push_back({
            .valid_from = entry["valid_from"].get<std::string>(),
            .value      = entry["value"].get<double>(),
        });
    }
    std::sort(rates.begin(), rates.end(),
              [](const TariffRate& a, const TariffRate& b) {
                  return a.valid_from < b.valid_from;
              });
    if (rates.empty()) {
        throw std::runtime_error("tariff rate schedule must have at least one entry");
    }
    return rates;
}

AppConfig load_config(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("Cannot open config: " + path);
    }
    const auto j = nlohmann::json::parse(f);

    AppConfig cfg;
    for (const auto& ep : j["listen"]) {
        cfg.listen.push_back({
            .address = ep["address"].get<std::string>(),
            .port    = ep["port"].get<uint16_t>(),
        });
    }
    cfg.data_dir = j["data_dir"];

    for (const auto& [area_id, aj] : j["areas"].items()) {
        AreaConfig area{
            .id           = area_id,
            .name         = aj["name"].get<std::string>(),
            .bidding_zone = aj["bidding_zone"].get<std::string>(),
            .timezone     = aj["timezone"].get<std::string>(),
            .energy_tax   = parse_rates(aj["energy_tax"]),
            .vat_rate     = aj["vat_rate"].get<double>(),
            .providers    = {},
        };

        for (const auto& [pid, pj] : aj["providers"].items()) {
            area.providers[pid] = ProviderConfig{
                .id             = pid,
                .name           = pj["name"].get<std::string>(),
                .delivery_fee   = parse_rates(pj["delivery_fee"]),
                .redelivery_fee = parse_rates(pj["redelivery_fee"]),
            };
        }

        cfg.areas[area_id] = std::move(area);
    }

    return cfg;
}
