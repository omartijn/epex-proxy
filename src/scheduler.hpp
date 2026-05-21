#pragma once
#include "config.hpp"
#include "store.hpp"
#include "time_util.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>

// Main per-area scheduler. Runs forever on the io_context:
//   - Fetches today on startup (unless already cached from disk)
//   - Spawns tomorrow fetch loop (retries every 5 min on failure)
//   - Spawns midnight promotion loop (resets tomorrow state at local midnight)
// Both loops honour their respective cancellation slots for graceful shutdown.
boost::asio::awaitable<void> run_scheduler(
    boost::asio::io_context&   ioc,
    PriceStore&                store,
    const AppConfig&           cfg,
    const AreaConfig&          area,
    std::string_view           token,
    boost::asio::cancellation_slot tomorrow_stop,
    boost::asio::cancellation_slot midnight_stop
);
