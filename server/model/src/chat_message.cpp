#include "model/chat_message.h"

std::string messageRoleToString(MessageRole r) {
    switch (r) {
        case MessageRole::User: return "user";
        case MessageRole::Assistant: return "assistant";
    }
    return "unknown";
}

nlohmann::json ChatMessage::toJson() const {
    return {
        {"id", id},
        {"user_id", userId},
        {"role", messageRoleToString(role)},
        {"content", content},
        {"session_id", sessionId},
        {"parent_id", parentId},
        {"created_at", createdAt}
    };
}

ChatMessage ChatMessage::fromJson(const nlohmann::json& j) {
    ChatMessage m;
    m.id = j.value("id", "");
    m.userId = j.value("user_id", "");
    m.content = j.value("content", "");
    m.sessionId = j.value("session_id", "");
    m.parentId = j.value("parent_id", "");
    m.createdAt = j.value("created_at", 0);

    std::string role_str = j.value("role", "user");
    if (role_str == "assistant") {
        m.role = MessageRole::Assistant;
    } else {
        m.role = MessageRole::User;
    }

    return m;
}