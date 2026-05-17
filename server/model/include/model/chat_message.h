#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

enum class MessageRole { User, Assistant };

std::string messageRoleToString(MessageRole r);

struct ChatMessage {
    std::string id;
    std::string userId;
    MessageRole role;
    std::string content;
    std::string sessionId;
    std::string parentId;
    int64_t createdAt = 0;

    nlohmann::json toJson() const;
    static ChatMessage fromJson(const nlohmann::json& j);
};