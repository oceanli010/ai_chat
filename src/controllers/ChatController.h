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

    uint64_t getUserIdFromToken(const HttpRequestPtr& req) {
        auto auth = req->getHeader("Authorization");
        if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);
        auto data = JWTUtils::verify_token(auth);
        return data.user_id;
    }

    void getHistory(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
        uint64_t user_id = getUserIdFromToken(req);

        int page = 1;
        int page_size = 200;
        auto param = req->getParameter("page");
        if (!param.empty()) page = std::stoi(param);
        param = req->getParameter("page_size");
        if (!param.empty()) page_size = std::stoi(param);

        int offset = (page - 1) * page_size;

        try {
            auto conn = MySQLClient::instance().acquire();

            auto count_stmt = conn->prepareStatement(
                "SELECT COUNT(*) FROM chat_messages WHERE user_id = ?");
            count_stmt->setUInt64(1, user_id);
            auto count_res = count_stmt->executeQuery();
            count_res->next();
            int total = count_res->getInt(1);

            auto stmt = conn->prepareStatement(
                "SELECT id, role, content, token_count, created_at "
                "FROM chat_messages WHERE user_id = ? "
                "ORDER BY created_at ASC LIMIT ? OFFSET ?");
            stmt->setUInt64(1, user_id);
            stmt->setInt(2, page_size);
            stmt->setInt(3, offset);
            auto res = stmt->executeQuery();

            Json::Value messages(Json::arrayValue);
            while (res->next()) {
                Json::Value msg;
                msg["id"] = static_cast<Json::Value::UInt64>(res->getUInt64("id"));
                msg["role"] = std::string(res->getString("role"));
                msg["content"] = std::string(res->getString("content"));
                msg["token_count"] = res->getUInt("token_count");
                msg["created_at"] = std::string(res->getString("created_at"));
                messages.append(msg);
            }

            MySQLClient::instance().release(std::move(conn));

            Json::Value result;
            result["code"] = 200;
            result["message"] = "操作成功";
            result["data"]["messages"] = messages;
            result["data"]["total"] = total;
            result["data"]["page"] = page;
            result["data"]["page_size"] = page_size;

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(result.toStyledString());
            callback(resp);

        } catch (const std::exception& e) {
            APP_LOG_ERROR("Get history error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    void sendMessage(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "请求格式无效"));
            callback(resp);
            return;
        }

        uint64_t user_id = getUserIdFromToken(req);
        std::string content = (*json)["content"].asString();

        if (content.empty()) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "消息内容不能为空"));
            callback(resp);
            return;
        }

        if (content.length() > 2000) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "消息内容不能超过2000字"));
            callback(resp);
            return;
        }

        try {
            auto conn = MySQLClient::instance().acquire();

            auto stmt = conn->prepareStatement(
                "INSERT INTO chat_messages (user_id, role, content) VALUES (?, 'user', ?)");
            stmt->setUInt64(1, user_id);
            stmt->setString(2, content);
            stmt->executeUpdate();

            std::string ai_reply = callAIService(content);

            auto ai_stmt = conn->prepareStatement(
                "INSERT INTO chat_messages (user_id, role, content) VALUES (?, 'assistant', ?)");
            ai_stmt->setUInt64(1, user_id);
            ai_stmt->setString(2, ai_reply);
            ai_stmt->executeUpdate();

            auto update = conn->prepareStatement(
                "UPDATE users SET total_chats = total_chats + 1 WHERE id = ?");
            update->setUInt64(1, user_id);
            update->executeUpdate();

            MySQLClient::instance().release(std::move(conn));

            Json::Value result;
            result["code"] = 200;
            result["message"] = "操作成功";
            result["data"]["reply"] = ai_reply;

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(result.toStyledString());
            callback(resp);

        } catch (const std::exception& e) {
            APP_LOG_ERROR("Send message error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    void clearHistory(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
        uint64_t user_id = getUserIdFromToken(req);

        try {
            auto conn = MySQLClient::instance().acquire();

            auto stmt = conn->prepareStatement(
                "DELETE FROM chat_messages WHERE user_id = ?");
            stmt->setUInt64(1, user_id);
            stmt->executeUpdate();

            MySQLClient::instance().release(std::move(conn));

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateSuccess("聊天记录已清除"));
            callback(resp);

        } catch (const std::exception& e) {
            APP_LOG_ERROR("Clear history error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    static void setAIAPIUrl(const std::string& url) { ai_api_url_ = url; }
    static void setAIAPIKey(const std::string& key) { ai_api_key_ = key; }
    static void setAIModel(const std::string& model) { ai_model_ = model; }

private:
    static std::string ai_api_url_;
    static std::string ai_api_key_;
    static std::string ai_model_;

    static std::string callAIService(const std::string& user_message) {
        if (ai_api_key_.empty()) {
            return "AI服务未配置，请在config.json中设置API密钥。";
        }

        auto client = HttpClient::newHttpClient(ai_api_url_);
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/v1/chat/completions");
        req->setMethod(HttpMethod::Post);
        req->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
        req->addHeader("Authorization", "Bearer " + ai_api_key_);

        Json::Value body;
        body["model"] = ai_model_;
        Json::Value msgs(Json::arrayValue);
        Json::Value msg;
        msg["role"] = "user";
        msg["content"] = user_message;
        msgs.append(msg);
        body["messages"] = msgs;

        req->setBody(body.toStyledString());

        try {
            auto result_pair = client->sendRequest(req, 30.0);
            auto resp = result_pair.second;
            if (result_pair.first != ReqResult::Ok || !resp) {
                return "AI服务请求失败，请稍后重试。";
            }
            auto resp_json = resp->getJsonObject();
            if (resp_json && (*resp_json)["choices"].isArray()) {
                auto& choices = (*resp_json)["choices"];
                if (choices.size() > 0) {
                    return choices[0]["message"]["content"].asString();
                }
            }
            return "AI服务返回格式异常，请稍后重试。";
        } catch (const std::exception& e) {
            APP_LOG_ERROR("AI service call failed: {}", e.what());
            return "AI服务调用失败，请检查API配置和网络连接。";
        }
    }

    static std::string generateError(int code, const std::string& message) {
        Json::Value result;
        result["code"] = code;
        result["message"] = message;
        return result.toStyledString();
    }

    static std::string generateSuccess(const std::string& message) {
        Json::Value result;
        result["code"] = 200;
        result["message"] = message;
        return result.toStyledString();
    }
};
