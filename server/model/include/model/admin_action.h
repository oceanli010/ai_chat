#pragma once

#include <string>
#include <cstdint>

struct AdminAction {
    int64_t id = 0;
    std::string adminId;
    std::string action;
    std::string targetId;
    std::string detail;
    std::string ipAddress;
    int64_t createdAt = 0;
};