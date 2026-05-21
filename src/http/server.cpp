#include "server.hpp"
#include "handlers.hpp"
#include "../config.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast.hpp>
#include <iostream>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = asio::ip::tcp;

static asio::awaitable<void> session(
    tcp::socket       socket,
    const PriceStore& store,
    const AppConfig&  cfg)
{
    beast::flat_buffer buf;
    try {
        for (;;) {
            http::request<http::string_body> req;
            co_await http::async_read(socket, buf, req, asio::use_awaitable);

            bool keep_alive = req.keep_alive() && !store.shutting_down;
            auto res        = handle_request(req, store, cfg);
            res.keep_alive(keep_alive);
            res.prepare_payload();

            co_await http::async_write(socket, res, asio::use_awaitable);
            if (!keep_alive) {
                break;
            }
        }
    } catch (const boost::system::system_error& e) {
        if (e.code() != http::error::end_of_stream) {
            std::cerr << "session: " << e.what() << '\n';
        }
    }

    // Socket closes on scope exit; the destructor sends the FIN.

}

static asio::awaitable<void> accept_loop(
    tcp::acceptor&    acceptor,
    asio::io_context& ioc,
    const PriceStore& store,
    const AppConfig&  cfg)
{
    for (;;) {
        boost::system::error_code ec;
        auto socket = co_await acceptor.async_accept(
            asio::redirect_error(asio::use_awaitable, ec));
        if (ec == asio::error::operation_aborted) {
            co_return;
        }
        if (ec) { std::cerr << "accept: " << ec.message() << '\n'; continue; }
        asio::co_spawn(ioc, session(std::move(socket), store, cfg), asio::detached);
    }
}

void run_server(
    asio::io_context&          ioc,
    std::vector<tcp::acceptor>& acceptors,
    const PriceStore&           store,
    const AppConfig&            cfg)
{
    for (auto& acc : acceptors) {
        asio::co_spawn(ioc, accept_loop(acc, ioc, store, cfg), asio::detached);
    }
}
