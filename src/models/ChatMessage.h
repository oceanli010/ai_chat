#pragma once

#include <string>
#include <cstdint>

struct ChatMessage {
    uint64_t id = 0;
    uint64_t user_id = 0;
    std::string role;
    std::string content;
    uint32_t token_count = 0;
    std::string created_at;
};
