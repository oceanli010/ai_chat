#pragma once

#include <string>
#include <nlohmann/json.hpp>

class UserService {
public:
    nlohmann::json sendVerificationCode(const std::string& email);
    nlohmann::json registerUser(const std::string& email, const std::string& code, const std::string& nickname, const std::string& password);
    nlohmann::json getProfile(const std::string& token);
    nlohmann::json updateNickname(const std::string& token, const std::string& nickname);
    nlohmann::json deleteAccount(const std::string& token);
};
