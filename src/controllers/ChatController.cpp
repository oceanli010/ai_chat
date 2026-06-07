#include "ChatController.h"

std::string ChatController::ai_api_url_ = "https://api.openai.com/v1/chat/completions";
std::string ChatController::ai_api_key_ = "";
std::string ChatController::ai_model_ = "gpt-3.5-turbo";

// getUserIdFromToken
// 功能：从 HTTP 请求的 Authorization 头中解析 JWT token 并提取用户 ID
// 参数：req - HTTP 请求对象
// 返回值：uint64_t - 用户 ID
uint64_t ChatController::getUserIdFromToken(const HttpRequestPtr& req) {
    auto auth = req->getHeader("Authorization");
    if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);
    auto data = JWTUtils::verify_token(auth);
    return data.user_id;
}

// getHistory
// 功能：获取当前用户的聊天历史记录
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：支持分页查询，按时间升序排列
void ChatController::getHistory(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback) {
    uint64_t user_id = std::stoull(req->getHeader("X-User-Id"));

    // 解析分页参数，默认第 1 页每页 200 条
    int page = 1;
    int page_size = 200;
    auto param = req->getParameter("page");
    if (!param.empty()) page = std::stoi(param);
    param = req->getParameter("page_size");
    if (!param.empty()) page_size = std::stoi(param);

    int offset = (page - 1) * page_size;

    try {
        auto conn = MySQLClient::instance().acquire();

        // 查询总消息数
        auto count_stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM chat_messages WHERE user_id = ?");
        count_stmt->setUInt64(1, user_id);
        auto count_res = count_stmt->executeQuery();
        count_res->next();
        int total = count_res->getInt(1);

        // 分页查询聊天记录，按时间升序
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

        Json::FastWriter writer;
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(writer.write(result));
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Get history error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

// sendMessage
// 功能：发送用户消息并获取 AI 回复
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：将用户消息存入数据库，调用 AI API 获取回复后异步保存并返回
void ChatController::sendMessage(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请求格式无效"));
        callback(resp);
        return;
    }

    uint64_t user_id = std::stoull(req->getHeader("X-User-Id"));
    std::string content = (*json)["content"].asString();

    if (content.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "消息内容不能为空"));
        callback(resp);
        return;
    }

    // 消息长度限制 2000 字
    if (content.length() > 2000) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "消息内容不能超过2000字"));
        callback(resp);
        return;
    }

    // 使用 shared_ptr 包装回调，以便在异步请求中安全使用
    auto cb_ptr = std::make_shared<std::function<void(const HttpResponsePtr&)>>(std::move(callback));

    try {
        auto conn = MySQLClient::instance().acquire();

        // 保存用户消息到数据库
        auto stmt = conn->prepareStatement(
            "INSERT INTO chat_messages (user_id, role, content) VALUES (?, 'user', ?)");
        stmt->setUInt64(1, user_id);
        stmt->setString(2, content);
        stmt->executeUpdate();

        // 使用 shared_ptr 管理数据库连接，以便在异步回调中释放
        auto conn_ptr = std::make_shared<std::unique_ptr<sql::Connection>>(std::move(conn));

        // 构建 AI API 请求
        auto client = HttpClient::newHttpClient(ai_api_url_);
        auto ai_req = HttpRequest::newHttpRequest();
        ai_req->setPath("/v1/chat/completions");
        ai_req->setMethod(HttpMethod::Post);
        ai_req->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
        ai_req->addHeader("Authorization", "Bearer " + ai_api_key_);

        Json::Value body;
        body["model"] = ai_model_;
        Json::Value msgs(Json::arrayValue);
        Json::Value msg;
        msg["role"] = "user";
        msg["content"] = content;
        msgs.append(msg);
        body["messages"] = msgs;
        ai_req->setBody(body.toStyledString());

        // 发送异步 AI 请求，超时 30 秒
        client->sendRequest(ai_req, [cb_ptr, conn_ptr, user_id](ReqResult result, const HttpResponsePtr& response) {
            if (result != ReqResult::Ok || !response) {
                MySQLClient::instance().release(std::move(*conn_ptr));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(502, "AI服务请求失败，请稍后重试。"));
                (*cb_ptr)(resp);
                return;
            }

            // 解析 AI 回复内容
            auto resp_json = response->getJsonObject();
            std::string ai_reply;
            if (resp_json && (*resp_json)["choices"].isArray() && (*resp_json)["choices"].size() > 0) {
                ai_reply = (*resp_json)["choices"][0]["message"]["content"].asString();
            } else {
                ai_reply = "AI服务返回格式异常，请稍后重试。";
            }

            try {
                auto& conn = *conn_ptr;
                // 保存 AI 回复到数据库
                auto ai_stmt = conn->prepareStatement(
                    "INSERT INTO chat_messages (user_id, role, content) VALUES (?, 'assistant', ?)");
                ai_stmt->setUInt64(1, user_id);
                ai_stmt->setString(2, ai_reply);
                ai_stmt->executeUpdate();

                // 更新用户的总聊天次数
                auto update = conn->prepareStatement(
                    "UPDATE users SET total_chats = total_chats + 1 WHERE id = ?");
                update->setUInt64(1, user_id);
                update->executeUpdate();

                MySQLClient::instance().release(std::move(*conn_ptr));
            } catch (const std::exception& e) {
                APP_LOG_ERROR("Save AI reply error: {}", e.what());
                MySQLClient::instance().release(std::move(*conn_ptr));
            }

            Json::Value result_json;
            Json::FastWriter writer;
            result_json["code"] = 200;
            result_json["message"] = "操作成功";
            result_json["data"]["reply"] = ai_reply;

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(writer.write(result_json));
            (*cb_ptr)(resp);
        }, 30.0);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Send message error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        (*cb_ptr)(resp);
    }
}

// clearHistory
// 功能：清空当前用户的所有聊天记录
// 参数：req - HTTP 请求对象；callback - 异步响应回调
void ChatController::clearHistory(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    uint64_t user_id = std::stoull(req->getHeader("X-User-Id"));

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

void ChatController::setAIAPIUrl(const std::string& url) { ai_api_url_ = url; }
void ChatController::setAIAPIKey(const std::string& key) { ai_api_key_ = key; }
void ChatController::setAIModel(const std::string& model) { ai_model_ = model; }

// generateError
// 功能：生成错误响应 JSON 字符串
// 参数：code - 错误状态码；message - 错误信息
// 返回值：string - JSON 格式的错误响应字符串
std::string ChatController::generateError(int code, const std::string& message) {
    Json::Value result;
    result["code"] = code;
    result["message"] = message;
    Json::FastWriter writer;
    return writer.write(result);
}

// generateSuccess
// 功能：生成成功响应 JSON 字符串
// 参数：message - 成功信息
// 返回值：string - JSON 格式的成功响应字符串
std::string ChatController::generateSuccess(const std::string& message) {
    Json::Value result;
    result["code"] = 200;
    result["message"] = message;
    Json::FastWriter writer;
    return writer.write(result);
}
