#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

class Protocol {
public:
    static nlohmann::json parseJsonBody(const http::request<http::string_body>& req);
    static http::response<http::string_body> makeJsonResponse(
        http::status status, const nlohmann::json& body);
    static http::response<http::string_body> makeErrorResponse(
        http::status status, const std::string& message);
    static std::string extractToken(const http::request<http::string_body>& req);
};