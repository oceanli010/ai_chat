#pragma once

#include "model/chat_message.h"
#include "data/connection_pool.h"
#include <string>
#include <vector>
#include <memory>

class ChatRepository {
public:
    explicit ChatRepository(std::shared_ptr<ConnectionPool> pool);

    bool saveMessage(const ChatMessage& msg);
    std::vector<ChatMessage> getSessionMessages(const std::string& sessionId, int limit);
    std::vector<ChatMessage> getUserMessages(const std::string& userId, int limit, int offset);
    bool deleteSession(const std::string& sessionId);
    bool deleteUserMessages(const std::string& userId);

private:
    std::shared_ptr<ConnectionPool> pool_;
};