#include "calculator.hpp"
#include <chrono>
#include <cmath>
#include <format>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string format_utc(std::chrono::system_clock::time_point tp) {
    auto ss = std::chrono::floor<std::chrono::seconds>(tp);
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", ss);
}

static std::string make_raw_json(const DayPrices& prices, const AreaConfig& area) {
    json j;
    j["date"]       = prices.date;
    j["area"]       = area.bidding_zone;
    j["resolution"] = "PT15M";
    j["currency"]   = "EUR";
    j["unit"]       = "MWh";
    j["prices"]     = json::array();

    constexpr auto slot_dur = std::chrono::minutes{15};
    for (const auto& slot : prices.slots) {
        j["prices"].push_back({
            {"start", format_utc(slot.start)},
            {"end",   format_utc(slot.start + slot_dur)},
            {"price", slot.epex_mwh}
        });
    }
    return j.dump();
}

static std::string make_provider_json(
    const DayPrices&     prices,
    const AreaConfig&    area,
    const ProviderConfig& provider,
    bool                 incl_vat)
{
    json j;
    j["date"]         = prices.date;
    j["provider"]     = provider.id;
    j["area"]         = area.id;
    j["currency"]     = "EUR";
    j["unit"]         = "kWh";
    j["vat_included"] = incl_vat;
    double et = lookup_rate(area.energy_tax,         prices.date);
    double df = lookup_rate(provider.delivery_fee,   prices.date);
    double rf = lookup_rate(provider.redelivery_fee, prices.date);

    j["components"]   = {
        {"energy_tax",    et},
        {"vat_rate",      area.vat_rate},
        {"delivery_fee",  df},
        {"redelivery_fee", rf}
    };
    j["prices"] = json::array();

    constexpr auto slot_dur = std::chrono::minutes{15};
    for (const auto& slot : prices.slots) {
        // VAT applies only to the spot price; energy_tax and delivery_fee
        // are stored as VAT-inclusive amounts (as published by providers/Belastingdienst).
        double spot_kwh  = slot.epex_mwh / 1000.0;
        double buy_incl  = spot_kwh * (1.0 + area.vat_rate)
                         + et
                         + df;
        double sell_incl = buy_incl - rf;
        // ex-VAT: remove VAT uniformly from the total (all components carry the same rate)
        double buy  = incl_vat ? buy_incl  : buy_incl  / (1.0 + area.vat_rate);
        double sell = incl_vat ? sell_incl : sell_incl / (1.0 + area.vat_rate);
        // Round to 5 decimal places (€/kWh, sub-cent precision)
        auto r5 = [](double v) { return std::round(v * 1e5) / 1e5; };
        j["prices"].push_back({
            {"start", format_utc(slot.start)},
            {"end",   format_utc(slot.start + slot_dur)},
            {"buy",   r5(buy)},
            {"sell",  r5(sell)}
        });
    }
    return j.dump();
}

CachedDay make_cached_day(const DayPrices& prices, const AreaConfig& area) {
    CachedDay cached;
    cached.prices   = prices;
    cached.raw_json = make_raw_json(prices, area);
    for (const auto& [pid, provider] : area.providers) {
        cached.provider_json[pid]        = make_provider_json(prices, area, provider, true);
        cached.provider_json_ex_vat[pid] = make_provider_json(prices, area, provider, false);
    }
    return cached;
}
