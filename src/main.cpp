#include "config.hpp"
#include "store.hpp"
#include "scheduler.hpp"
#include "http/server.hpp"
#include <boost/asio.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <cstdlib>
#include <iostream>
#include <unordered_map>

namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

int main(int argc, char* argv[]) {
    try {
        const std::string config_path = argc > 1 ? argv[1] : "config.json";
        const AppConfig   cfg         = load_config(config_path);

        const char* token_env = std::getenv("ENTSOE_TOKEN");
        if (!token_env || !*token_env) {
            std::cerr << "ENTSOE_TOKEN is not set\n";
            return 1;
        }
        const std::string token{token_env};

        PriceStore store;
        load_from_disk(store, cfg);

        asio::io_context ioc;

        // Acceptors live here so the signal handler can cancel them without
        // lifetime concerns — they outlive every coroutine.
        std::vector<tcp::acceptor> acceptors;
        for (const auto& ep : cfg.listen) {
            auto endpoint = tcp::endpoint{asio::ip::make_address(ep.address), ep.port};
            acceptors.emplace_back(ioc.get_executor(), endpoint);
            std::cout << "Listening on " << ep.address << ':' << ep.port << '\n';
        }

        // Per-area cancellation signals for the two scheduler loops.
        struct AreaSignals {
            asio::cancellation_signal tomorrow_fetch;
            asio::cancellation_signal midnight;
        };
        std::unordered_map<std::string, AreaSignals> area_signals;

        for (const auto& [area_id, area] : cfg.areas) {
            auto& sigs = area_signals[area_id];
            asio::co_spawn(
                ioc,
                run_scheduler(ioc, store, cfg, area, token,
                              sigs.tomorrow_fetch.slot(), sigs.midnight.slot()),
                asio::detached);
        }

        // Shared stop signal for HTTP sessions. Held "never expiring" until
        // shutdown; sessions race their reads against it (see run_server).
        asio::steady_timer shutdown_timer{ioc};
        shutdown_timer.expires_at(asio::steady_timer::time_point::max());

        run_server(ioc, acceptors, shutdown_timer, store, cfg);

        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](boost::system::error_code ec, int sig) {
            if (ec) {
                return;
            }
            std::cout << "\nShutting down (signal " << sig << ")...\n";
            store.shutting_down = true;
            for (auto& acc : acceptors) {
                acc.cancel();
            }
            // Expire (don't just cancel) so sessions that loop back to a fresh
            // wait after this point still see the deadline already passed.
            shutdown_timer.expires_at(asio::steady_timer::time_point::min());
            for (auto& [id, sigs] : area_signals) {
                sigs.tomorrow_fetch.emit(asio::cancellation_type::terminal);
                sigs.midnight.emit(asio::cancellation_type::terminal);
            }
        });

        ioc.run();
        std::cout << "Shutdown complete.\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
}
