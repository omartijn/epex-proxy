#include "handlers.hpp"
#include "../time_util.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace http = boost::beast::http;
using json     = nlohmann::json;

// ---------------------------------------------------------------------------
// Response helpers
// ---------------------------------------------------------------------------

static http::response<http::string_body> json_response(
    http::status                             status,
    std::string                              body,
    const http::request<http::string_body>&  req)
{
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type,                  "application/json");
    res.set(http::field::access_control_allow_origin,   "*");
    res.body() = std::move(body);
    return res;
}

static http::response<http::string_body> not_found(
    std::string_view                        reason,
    const http::request<http::string_body>& req)
{
    return json_response(http::status::not_found,
        json{{"error", "not_found"}, {"reason", reason}}.dump(), req);
}

static http::response<http::string_body> method_not_allowed(
    const http::request<http::string_body>& req)
{
    return json_response(http::status::method_not_allowed,
        json{{"error", "method_not_allowed"}}.dump(), req);
}

// ---------------------------------------------------------------------------
// Path / query parsing
// ---------------------------------------------------------------------------

static std::vector<std::string> split_path(std::string_view path) {
    std::vector<std::string> parts;
    std::size_t start = path.starts_with('/') ? 1 : 0;
    while (start < path.size()) {
        auto end = path.find('/', start);
        if (end == std::string_view::npos) {
            end = path.size();
        }
        if (end > start) {
            parts.emplace_back(path.substr(start, end - start));
        }
        start = end + 1;
    }
    return parts;
}

static std::pair<std::string_view, std::string_view> split_target(std::string_view target) {
    auto q = target.find('?');
    if (q == std::string_view::npos) {
        return {target, {}};
    }
    return {target.substr(0, q), target.substr(q + 1)};
}

static bool has_vat_false(std::string_view query) {
    return query.find("vat=false") != std::string_view::npos;
}

static std::string resolve_date(std::string_view token, const AreaConfig& area) {
    if (token == "today") {
        return local_date(area);
    }
    if (token == "tomorrow") {
        return local_date_tomorrow(area);
    }
    return std::string(token);
}

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

static http::response<http::string_body> handle_health(
    const PriceStore&                       store,
    const http::request<http::string_body>& req)
{
    return json_response(http::status::ok, store.health_json, req);
}

static http::response<http::string_body> handle_providers(
    std::string_view                        area_id,
    const AppConfig&                        cfg,
    const http::request<http::string_body>& req)
{
    auto ait = cfg.areas.find(std::string(area_id));
    if (ait == cfg.areas.end()) {
        return not_found("Unknown area", req);
    }

    const auto& area = ait->second;
    std::string today = local_date(area);
    json j;
    j["area"]       = area_id;
    j["energy_tax"] = lookup_rate(area.energy_tax, today);
    j["vat_rate"]   = area.vat_rate;
    j["providers"]  = json::object();
    for (const auto& [pid, p] : area.providers) {
        j["providers"][pid] = {
            {"name",           p.name},
            {"delivery_fee",   lookup_rate(p.delivery_fee,   today)},
            {"redelivery_fee", lookup_rate(p.redelivery_fee, today)}
        };
    }
    return json_response(http::status::ok, j.dump(), req);
}

// ---------------------------------------------------------------------------
// Main dispatcher
// ---------------------------------------------------------------------------

http::response<http::string_body> handle_request(
    const http::request<http::string_body>& req,
    const PriceStore&                       store,
    const AppConfig&                        cfg)
{
    if (req.method() != http::verb::get) {
        return method_not_allowed(req);
    }

    auto [path, query] = split_target(req.target());
    bool ex_vat        = has_vat_false(query);

    if (path == "/health") {
        return handle_health(store, req);
    }

    auto parts = split_path(path);

    // /v1/{area}/providers
    // /v1/{area}/raw/{date}
    // /v1/{area}/{provider}/{date}
    if (parts.size() >= 2 && parts[0] == "v1") {
        const auto& area_id = parts[1];
        auto acit = cfg.areas.find(area_id);
        if (acit == cfg.areas.end()) {
            return not_found("Unknown area", req);
        }
        const auto& area = acit->second;

        if (parts.size() == 3 && parts[2] == "providers") {
            return handle_providers(area_id, cfg, req);
        }

        if (parts.size() == 4) {
            const auto& segment  = parts[2];
            const auto  date     = resolve_date(parts[3], area);

            if (segment == "raw") {
                const auto* day = store.find_day(area_id, date);
                if (!day) {
                    return not_found("No data for this date", req);
                }
                return json_response(http::status::ok, day->raw_json, req);
            }

            if (area.providers.find(segment) == area.providers.end()) {
                return not_found("Unknown provider", req);
            }

            const auto* day = store.find_day(area_id, date);
            if (!day) {
                return not_found("No data for this date", req);
            }

            const auto& jmap = ex_vat ? day->provider_json_ex_vat : day->provider_json;
            auto jit = jmap.find(segment);
            if (jit == jmap.end()) {
                return not_found("No cached response", req);
            }

            return json_response(http::status::ok, jit->second, req);
        }
    }

    return not_found("Unknown endpoint", req);
}
