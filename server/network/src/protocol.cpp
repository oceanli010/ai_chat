#include "network/protocol.h"

nlohmann::json Protocol::parseJsonBody(const http::request<http::string_body>& req) {
    return nlohmann::json::parse(req.body());
}

http::response<http::string_body> Protocol::makeJsonResponse(
    http::status status, const nlohmann::json& body) {
    http::response<http::string_body> res{status, 11};
    res.set(http::field::content_type, "application/json");
    res.body() = body.dump();
    res.prepare_payload();
    return res;
}

http::response<http::string_body> Protocol::makeErrorResponse(
    http::status status, const std::string& message) {
    nlohmann::json err = {{"error", message}};
    return makeJsonResponse(status, err);
}

std::string Protocol::extractToken(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) return "";
    std::string auth = std::string(it->value());
    const std::string prefix = "Bearer ";
    if (auth.size() > prefix.size() && auth.substr(0, prefix.size()) == prefix) {
        return auth.substr(prefix.size());
    }
    return "";
}