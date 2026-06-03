#pragma once

#include <string>
#include <cstdint>

// ChatMessage
// 聊天消息数据模型，对应数据库中 chat_messages 表的字段结构
struct ChatMessage {
    uint64_t id = 0;           // 消息唯一标识
    uint64_t user_id = 0;      // 所属用户的 ID
    std::string role;          // 消息角色（user / assistant / system）
    std::string content;       // 消息内容
    uint32_t token_count = 0;  // 消息的 Token 数量
    std::string created_at;    // 消息创建时间
};
