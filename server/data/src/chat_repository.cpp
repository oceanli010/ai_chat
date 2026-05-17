#include "data/chat_repository.h"
#include "infrastructure/logger.h"
#include <chrono>

ChatRepository::ChatRepository(std::shared_ptr<ConnectionPool> pool)
    : pool_(std::move(pool)) {}

bool ChatRepository::saveMessage(const ChatMessage& msg) {
    LOG_INFO("ChatRepository::saveMessage: session={}, user={}", msg.sessionId, msg.userId);

    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return false;
    }

    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return false;
    }

    try {
        std::string query = "INSERT INTO chat_messages (id, user_id, role, content, session_id, parent_id, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, msg.id);
        stmt->setString(2, msg.userId);
        stmt->setString(3, messageRoleToString(msg.role));
        stmt->setString(4, msg.content);
        stmt->setString(5, msg.sessionId);
        stmt->setString(6, msg.parentId);
        stmt->setInt64(7, msg.createdAt);

        return stmt->executeUpdate() > 0;
    } catch (const std::exception& e) {
        LOG_ERROR("ChatRepository::saveMessage error: {}", e.what());
    }

    return false;
}

std::vector<ChatMessage> ChatRepository::getSessionMessages(const std::string& sessionId, int limit) {
    LOG_INFO("ChatRepository::getSessionMessages: session={}, limit={}", sessionId, limit);

    std::vector<ChatMessage> messages;

    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return messages;
    }

    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return messages;
    }

    try {
        std::string query = "SELECT id, user_id, role, content, session_id, parent_id, created_at FROM chat_messages WHERE session_id = ? ORDER BY created_at ASC LIMIT ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, sessionId);
        stmt->setInt(2, limit);

        auto result = stmt->executeQuery();
        while (result->next()) {
            ChatMessage msg;
            msg.id = result->getString("id");
            msg.userId = result->getString("user_id");
            std::string role = result->getString("role");
            msg.role = (role == "assistant") ? MessageRole::Assistant : MessageRole::User;
            msg.content = result->getString("content");
            msg.sessionId = result->getString("session_id");
            msg.parentId = result->getString("parent_id");
            msg.createdAt = result->getInt64("created_at");
            messages.push_back(msg);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ChatRepository::getSessionMessages error: {}", e.what());
    }

    return messages;
}

std::vector<ChatMessage> ChatRepository::getUserMessages(const std::string& userId, int limit, int offset) {
    LOG_INFO("ChatRepository::getUserMessages: user={}, limit={}, offset={}", userId, limit, offset);

    std::vector<ChatMessage> messages;

    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return messages;
    }

    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return messages;
    }

    try {
        std::string query = "SELECT id, user_id, role, content, session_id, parent_id, created_at FROM chat_messages WHERE user_id = ? ORDER BY created_at ASC LIMIT ? OFFSET ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, userId);
        stmt->setInt(2, limit);
        stmt->setInt(3, offset);

        auto result = stmt->executeQuery();
        while (result->next()) {
            ChatMessage msg;
            msg.id = result->getString("id");
            msg.userId = result->getString("user_id");
            std::string role = result->getString("role");
            msg.role = (role == "assistant") ? MessageRole::Assistant : MessageRole::User;
            msg.content = result->getString("content");
            msg.sessionId = result->getString("session_id");
            msg.parentId = result->getString("parent_id");
            msg.createdAt = result->getInt64("created_at");
            messages.push_back(msg);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ChatRepository::getUserMessages error: {}", e.what());
    }

    return messages;
}

bool ChatRepository::deleteSession(const std::string& sessionId) {
    LOG_INFO("ChatRepository::deleteSession: {}", sessionId);

    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return false;
    }

    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return false;
    }

    try {
        std::string query = "DELETE FROM chat_messages WHERE session_id = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, sessionId);

        return stmt->executeUpdate() > 0;
    } catch (const std::exception& e) {
        LOG_ERROR("ChatRepository::deleteSession error: {}", e.what());
    }

    return false;
}

bool ChatRepository::deleteUserMessages(const std::string& userId) {
    LOG_INFO("ChatRepository::deleteUserMessages: {}", userId);

    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return false;
    }

    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return false;
    }

    try {
        std::string query = "DELETE FROM chat_messages WHERE user_id = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, userId);

        return stmt->executeUpdate() > 0;
    } catch (const std::exception& e) {
        LOG_ERROR("ChatRepository::deleteUserMessages error: {}", e.what());
    }

    return false;
}