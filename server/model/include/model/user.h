#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

enum class UserStatus { Active, Banned };

std::string userStatusToString(UserStatus s);

struct User {
    std::string id;
    std::string email;
    std::string passwordHash;
    std::string nickname;
    std::string role;
    UserStatus status = UserStatus::Active;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    int64_t lastLoginAt = 0;

    bool isBanned() const { return status == UserStatus::Banned; }
    nlohmann::json toJson() const;
    static User fromJson(const nlohmann::json& j);
};