#pragma once
#include "../config.hpp"
#include "../store.hpp"
#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

http::response<http::string_body> handle_request(
    const http::request<http::string_body>& req,
    const PriceStore&                       store,
    const AppConfig&                        cfg
);
