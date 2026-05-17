#pragma once

#include <string>
#include <nlohmann/json.hpp>

class AdminService {
public:
    nlohmann::json getUsers(const std::string& token, int page, int limit, const std::string& search);
    nlohmann::json banUser(const std::string& token, const std::string& userId, bool ban);
    nlohmann::json getStats(const std::string& token);
};
