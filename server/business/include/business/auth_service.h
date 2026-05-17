#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

class AuthService {
public:
    nlohmann::json login(const std::string& email, const std::string& password);
    nlohmann::json refresh(const std::string& refreshToken);
    bool logout(const std::string& accessToken);
};