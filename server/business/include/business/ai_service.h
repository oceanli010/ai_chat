#pragma once

#include <string>
#include <nlohmann/json.hpp>

class AiService {
public:
    std::string chat(const std::string& prompt, const std::string& sessionId);
    nlohmann::json getModels();
};