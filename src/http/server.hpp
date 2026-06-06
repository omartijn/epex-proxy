#pragma once
#include "../config.hpp"
#include "../store.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <vector>

// Spawn one accept-loop coroutine per acceptor. Returns immediately;
// the loops run independently on the io_context until cancelled via acc.cancel().
//
// `shutdown` is a shared stop signal: each session races its (possibly idle)
// read against shutdown.async_wait(), so expiring the timer into the past
// unblocks keep-alive connections parked waiting for their next request.
void run_server(
    boost::asio::io_context&                     ioc,
    std::vector<boost::asio::ip::tcp::acceptor>& acceptors,
    boost::asio::steady_timer&                   shutdown,
    const PriceStore&                            store,
    const AppConfig&                             cfg
);
