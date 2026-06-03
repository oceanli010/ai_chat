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

// ChatController
// 功能：处理用户与 AI 聊天相关的业务逻辑，包括消息收发和历史记录管理
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
    static std::string ai_api_url_; // AI 服务的 API 地址
    static std::string ai_api_key_; // AI 服务的 API 密钥
    static std::string ai_model_;   // AI 模型名称

    static std::string generateError(int code, const std::string& message);
    static std::string generateSuccess(const std::string& message);
};
