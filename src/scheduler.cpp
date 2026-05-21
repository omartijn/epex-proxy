#include "scheduler.hpp"
#include "calculator.hpp"
#include "entsoe.hpp"
#include "parser.hpp"
#include "store.hpp"
#include "time_util.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <chrono>
#include <format>
#include <iostream>

namespace asio = boost::asio;
using namespace std::chrono_literals;

// Advance a YYYY-MM-DD date by one day
static std::string date_plus_one(std::string_view ymd) {
    int      y = std::stoi(std::string(ymd.substr(0, 4)));
    unsigned m = static_cast<unsigned>(std::stoi(std::string(ymd.substr(5, 2))));
    unsigned d = static_cast<unsigned>(std::stoi(std::string(ymd.substr(8, 2))));
    auto next  = std::chrono::sys_days{
        std::chrono::year_month_day{
            std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}
        }
    } + std::chrono::days{1};
    return std::format("{:%Y-%m-%d}", std::chrono::year_month_day{next});
}

// ---------------------------------------------------------------------------
// Fetch
// ---------------------------------------------------------------------------

static asio::awaitable<bool> do_fetch(
    PriceStore&       store,
    const AppConfig&  cfg,
    const AreaConfig& area,
    std::string_view  token,
    std::string_view  date_local,
    FetchState&       state)
{
    state.status       = FetchStatus::pending;
    state.last_attempt = std::chrono::system_clock::now();
    ++state.attempts;

    try {
        auto result = co_await fetch_entsoe(
            token,
            area.bidding_zone,
            entsoe_period_utc(area, date_local),
            entsoe_period_utc(area, date_plus_one(date_local)));

        state.last_http_status = result.http_status;

        if (result.http_status != 200) {
            state.last_error_body = result.body.substr(0, 512);
            std::cerr << '[' << area.id << "] fetch " << date_local
                      << " failed: HTTP " << result.http_status << '\n';
            regenerate_health(store, cfg);
            co_return false;
        }

        state.last_error_body.reset();
        auto prices = parse_entsoe_xml(result.body, date_local);
        auto cached = make_cached_day(prices, area);
        store.areas[area.id].days[std::string(date_local)] = std::move(cached);
        save_to_disk(cfg.data_dir, area.id, prices);
        state.status = FetchStatus::ok;

        std::cout << '[' << area.id << "] fetched " << date_local
                  << " (" << prices.slots.size() << " slots)\n";
        regenerate_health(store, cfg);
        co_return true;

    } catch (const std::exception& e) {
        state.last_http_status.reset();
        state.last_error_body = std::string("exception: ") + e.what();
        std::cerr << '[' << area.id << "] fetch exception: " << e.what() << '\n';
        regenerate_health(store, cfg);
        co_return false;
    }
}

// ---------------------------------------------------------------------------
// Scheduler coroutines
// ---------------------------------------------------------------------------

static asio::awaitable<void> tomorrow_fetch_loop(
    PriceStore&       store,
    const AppConfig&  cfg,
    const AreaConfig& area,
    std::string_view  token)
{
    auto& state = store.areas[area.id].tomorrow_fetch;
    asio::system_timer timer{co_await asio::this_coro::executor};

    try {
        if (state.status == FetchStatus::ok) {
            timer.expires_at(next_fetch_time(area));
        } else {
            auto next = next_fetch_time(area);
            auto now  = std::chrono::system_clock::now();
            timer.expires_at(next - now > std::chrono::hours{13} ? now : next);
        }
        co_await timer.async_wait(asio::use_awaitable);

        for (;;) {
            std::string tomorrow = local_date_tomorrow(area);
            bool ok = co_await do_fetch(store, cfg, area, token, tomorrow, state);

            timer.expires_at(ok ? next_fetch_time(area) : std::chrono::system_clock::now() + 5min);
            co_await timer.async_wait(asio::use_awaitable);
        }
    } catch (const boost::system::system_error& e) {
        if (e.code() != asio::error::operation_aborted) {
            throw;
        }
    }
}

static asio::awaitable<void> midnight_loop(
    PriceStore&       store,
    const AppConfig&  cfg,
    const AreaConfig& area)
{
    asio::system_timer timer{co_await asio::this_coro::executor};

    try {
        for (;;) {
            timer.expires_at(next_local_midnight(area));
            co_await timer.async_wait(asio::use_awaitable);

            store.areas[area.id].tomorrow_fetch = FetchState{};
            regenerate_health(store, cfg);
            std::cout << '[' << area.id << "] midnight: day promoted\n";
        }
    } catch (const boost::system::system_error& e) {
        if (e.code() != asio::error::operation_aborted) {
            throw;
        }
    }
}

asio::awaitable<void> run_scheduler(
    asio::io_context&    ioc,
    PriceStore&          store,
    const AppConfig&     cfg,
    const AreaConfig&    area,
    std::string_view     token,
    asio::cancellation_slot tomorrow_stop,
    asio::cancellation_slot midnight_stop)
{
    store.areas.try_emplace(area.id);
    auto& astore = store.areas[area.id];

    std::string today    = local_date(area);
    std::string tomorrow = local_date_tomorrow(area);

    if (!astore.days.contains(today)) {
        co_await do_fetch(store, cfg, area, token, today, astore.today_fetch);
    } else {
        astore.today_fetch.status = FetchStatus::ok;
        regenerate_health(store, cfg);
    }

    if (astore.days.contains(tomorrow)) {
        astore.tomorrow_fetch.status = FetchStatus::ok;
        regenerate_health(store, cfg);
    }

    asio::co_spawn(ioc, tomorrow_fetch_loop(store, cfg, area, token),
                   asio::bind_cancellation_slot(tomorrow_stop, asio::detached));
    asio::co_spawn(ioc, midnight_loop(store, cfg, area),
                   asio::bind_cancellation_slot(midnight_stop, asio::detached));
}
