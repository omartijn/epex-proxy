#include "entsoe.hpp"
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <format>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ssl   = asio::ssl;
using tcp       = asio::ip::tcp;

static constexpr std::string_view k_host    = "web-api.tp.entsoe.eu";
static constexpr std::string_view k_port    = "443";
static constexpr int               k_version = 11;

asio::awaitable<EntsoeFetchResult> fetch_entsoe(
    std::string_view token,
    std::string_view bidding_zone,
    std::string_view period_start,
    std::string_view period_end)
{
    auto executor = co_await asio::this_coro::executor;

    ssl::context ssl_ctx{ssl::context::tlsv12_client};
    ssl_ctx.set_default_verify_paths();
    ssl_ctx.set_verify_mode(ssl::verify_peer);

    tcp::resolver resolver{executor};
    auto endpoints = co_await resolver.async_resolve(k_host, k_port, asio::use_awaitable);

    beast::ssl_stream<beast::tcp_stream> stream{executor, ssl_ctx};
    SSL_set_tlsext_host_name(stream.native_handle(), std::string(k_host).c_str());

    co_await beast::get_lowest_layer(stream).async_connect(endpoints, asio::use_awaitable);
    co_await stream.async_handshake(ssl::stream_base::client, asio::use_awaitable);

    auto target = std::format(
        "/api?documentType=A44"
        "&in_Domain={0}&out_Domain={0}"
        "&periodStart={1}&periodEnd={2}"
        "&securityToken={3}",
        bidding_zone, period_start, period_end, token);

    http::request<http::empty_body> req{http::verb::get, target, k_version};
    req.set(http::field::host,       k_host);
    req.set(http::field::user_agent, "epex-proxy/1.0");
    req.set(http::field::accept,     "application/xml");

    co_await http::async_write(stream, req, asio::use_awaitable);

    beast::flat_buffer buf;
    http::response<http::string_body> res;
    co_await http::async_read(stream, buf, res, asio::use_awaitable);

    co_return EntsoeFetchResult{
        .http_status = static_cast<int>(res.result_int()),
        .body        = std::move(res.body())
    };
}
