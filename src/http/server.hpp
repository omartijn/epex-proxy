#pragma once
#include "../config.hpp"
#include "../store.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <vector>

// Spawn one accept-loop coroutine per acceptor. Returns immediately;
// the loops run independently on the io_context until cancelled via acc.cancel().
void run_server(
    boost::asio::io_context&                     ioc,
    std::vector<boost::asio::ip::tcp::acceptor>& acceptors,
    const PriceStore&                            store,
    const AppConfig&                             cfg
);
