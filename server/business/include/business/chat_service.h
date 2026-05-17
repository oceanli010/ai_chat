#pragma once

#include <string>
#include <nlohmann/json.hpp>

class ChatService {
public:
    std::string createSession(const std::string& userId);
    nlohmann::json sendMessage(const std::string& sessionId, const std::string& userId, const std::string& content);
    nlohmann::json getHistory(const std::string& userId, int limit, int offset);
    bool clearHistory(const std::string& userId);
};