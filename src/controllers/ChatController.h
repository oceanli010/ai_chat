#pragma once

#include <drogon/drogon.h>
#include <drogon/HttpController.h>
#include <string>
#include <chrono>
#include <ctime>
#include "database/MySQLClient.h"
#include "utils/JWTUtils.h"
#include "utils/Logger.h"

using namespace drogon;

class ChatController : public HttpController<ChatController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ChatController::getHistory, "/api/chat/history", Get);
    ADD_METHOD_TO(ChatController::sendMessage, "/api/chat/send", Post);
    ADD_METHOD_TO(ChatController::clearHistory, "/api/chat/clear", Delete);
    METHOD_LIST_END

    uint64_t getUserIdFromToken(const HttpRequestPtr& req);

    void getHistory(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);

    void sendMessage(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);

    void clearHistory(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback);

    static void setAIAPIUrl(const std::string& url);
    static void setAIAPIKey(const std::string& key);
    static void setAIModel(const std::string& model);

private:
    static std::string ai_api_url_;
    static std::string ai_api_key_;
    static std::string ai_model_;

    static std::string callAIService(const std::string& user_message);
    static std::string generateError(int code, const std::string& message);
    static std::string generateSuccess(const std::string& message);
};
