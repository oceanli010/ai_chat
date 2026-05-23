#pragma once

#include <string>
#include <cstdint>

struct User {
    uint64_t id = 0;
    std::string username;
    std::string password_hash;
    std::string password_salt;
    std::string email;
    std::string nickname;
    std::string role = "user";
    std::string status = "active";
    uint32_t total_chats = 0;
    std::string created_at;
    std::string updated_at;
    std::string deleted_at;
};
