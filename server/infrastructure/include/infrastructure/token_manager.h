#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

struct TokenPayload {
    std::string user_id;
    std::string role;
    int64_t exp;
};

class TokenManager {
public:
    explicit TokenManager(const std::string& secret);
    std::string generateAccessToken(const std::string& userId, const std::string& role);
    std::string generateRefreshToken(const std::string& userId);
    std::optional<TokenPayload> validateToken(const std::string& token);
    void invalidateToken(const std::string& token);

private:
    std::string secret_;
    std::string createToken(const nlohmann::json& payload);
    static int64_t now();
};