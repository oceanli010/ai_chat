#include "business/chat_service.h"
#include "business/ai_service.h"
#include "business/service_locator.h"
#include "infrastructure/logger.h"
#include "infrastructure/crypto_util.h"
#include "model/chat_message.h"
#include "data/chat_repository.h"
#include <chrono>

std::string ChatService::createSession(const std::string& userId) {
    LOG_INFO("ChatService::createSession: {}", userId);
    return "session_" + userId;
}

nlohmann::json ChatService::sendMessage(const std::string& sessionId, const std::string& userId, const std::string& content) {
    LOG_INFO("ChatService::sendMessage: session={}, user={}", sessionId, userId);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    ChatMessage userMsg;
    userMsg.id = CryptoUtil::generateUUID();
    userMsg.userId = userId;
    userMsg.role = MessageRole::User;
    userMsg.content = content;
    userMsg.sessionId = sessionId;
    userMsg.createdAt = now;

    auto repo = ServiceLocator::instance().chatRepository();
    if (repo) {
        repo->saveMessage(userMsg);
    }

    AiService ai;
    std::string reply = ai.chat(content, sessionId);

    ChatMessage assistantMsg;
    assistantMsg.id = CryptoUtil::generateUUID();
    assistantMsg.userId = userId;
    assistantMsg.role = MessageRole::Assistant;
    assistantMsg.content = reply;
    assistantMsg.sessionId = sessionId;
    assistantMsg.parentId = userMsg.id;
    assistantMsg.createdAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (repo) {
        repo->saveMessage(assistantMsg);
    }

    return {
        {"content", reply},
        {"id", assistantMsg.id}
    };
}

nlohmann::json ChatService::getHistory(const std::string& userId, int limit, int offset) {
    LOG_INFO("ChatService::getHistory: user={}, limit={}, offset={}", userId, limit, offset);

    auto repo = ServiceLocator::instance().chatRepository();
    if (!repo) {
        return nlohmann::json::array();
    }

    auto messages = repo->getUserMessages(userId, limit, offset);
    nlohmann::json result = nlohmann::json::array();
    for (const auto& msg : messages) {
        result.push_back(msg.toJson());
    }
    return result;
}

bool ChatService::clearHistory(const std::string& userId) {
    LOG_INFO("ChatService::clearHistory: {}", userId);

    auto repo = ServiceLocator::instance().chatRepository();
    if (!repo) {
        return false;
    }

    return repo->deleteUserMessages(userId);
}