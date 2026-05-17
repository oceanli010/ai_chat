#include "business/ai_service.h"
#include "infrastructure/logger.h"
#include "infrastructure/config.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total);
    return total;
}

std::string AiService::chat(const std::string& prompt, const std::string& sessionId) {
    LOG_INFO("AiService::chat: session={}, prompt_length={}", sessionId, prompt.size());

    auto& cfg = Config::instance().ai;
    if (cfg.endpoint.empty() || cfg.api_key.empty()) {
        LOG_ERROR("AI config incomplete: endpoint={}, api_key={}", cfg.endpoint, cfg.api_key.empty() ? "empty" : "set");
        return "AI service not configured";
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize curl");
        return "AI service temporarily unavailable";
    }

    std::string url = cfg.endpoint;
    if (url.back() != '/') url += '/';
    url += "chat/completions";

    nlohmann::json request_body = {
        {"model", cfg.model},
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", prompt}}
        })},
        {"max_tokens", cfg.max_tokens},
        {"stream", false}
    };

    std::string request_str = request_body.dump();
    std::string response_str;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + cfg.api_key).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_str.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(cfg.timeout_seconds));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("AI API request failed: {}", curl_easy_strerror(res));
        return "AI service request failed: " + std::string(curl_easy_strerror(res));
    }

    try {
        auto j = nlohmann::json::parse(response_str);
        if (j.contains("choices") && !j["choices"].empty()) {
            std::string content = j["choices"][0]["message"]["content"];
            LOG_INFO("AiService::chat response received, length={}", content.size());
            return content;
        }
        if (j.contains("error")) {
            std::string err = j["error"].value("message", "Unknown error");
            LOG_ERROR("AI API error: {}", err);
            return "AI error: " + err;
        }
        LOG_ERROR("Unexpected AI API response: {}", response_str.substr(0, 200));
        return "Unexpected AI response format";
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse AI response: {}", e.what());
        return "Failed to parse AI response";
    }
}

nlohmann::json AiService::getModels() {
    try {
        auto& cfg = Config::instance().ai;
        std::string url = cfg.endpoint;
        if (url.back() != '/') url += '/';
        url += "models";

        CURL* curl = curl_easy_init();
        if (!curl) return nlohmann::json::array({cfg.model});

        std::string response_str;
        struct curl_slist* headers = nullptr;
        std::string auth_hdr = "Authorization: Bearer " + cfg.api_key;
        headers = curl_slist_append(headers, auth_hdr.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            auto j = nlohmann::json::parse(response_str);
            if (j.contains("data")) {
                nlohmann::json models = nlohmann::json::array();
                for (auto& m : j["data"]) {
                    models.push_back(m.value("id", ""));
                }
                return models;
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN("Failed to fetch models: {}", e.what());
    }

    return nlohmann::json::array({Config::instance().ai.model});
}
