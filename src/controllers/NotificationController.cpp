#include "NotificationController.h"

void NotificationController::handleNewConnection(const HttpRequestPtr& req,
                                                   const WebSocketConnectionPtr& conn) {
    auto token = req->getParameter("token");
    if (token.empty()) {
        auto auth = req->getHeader("Authorization");
        if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ") {
            token = auth.substr(7);
        }
    }

    if (token.empty()) {
        conn->shutdown();
        return;
    }

    try {
        auto data = JWTUtils::verify_token(token);
        auto user_id = data.user_id;
        auto session_key = "session:" + token;
        if (!RedisClient::instance().exists(session_key)) {
            conn->shutdown();
            return;
        }

        conn->setContext(std::make_shared<UserContext>(user_id, data.username));

        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[user_id].push_back(conn);
        }

        APP_LOG_INFO("WebSocket connected for user {} ({})", user_id, data.username);
    } catch (const std::exception& e) {
        APP_LOG_WARN("WebSocket auth failed: {}", e.what());
        conn->shutdown();
    }
}

void NotificationController::handleNewMessage(const WebSocketConnectionPtr& conn,
                                                std::string&& message,
                                                const WebSocketMessageType& type) {
}

void NotificationController::handleConnectionClosed(const WebSocketConnectionPtr& conn) {
    auto ctx = conn->getContext<UserContext>();
    if (ctx) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(ctx->user_id);
        if (it != connections_.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), conn), vec.end());
            if (vec.empty()) {
                connections_.erase(it);
            }
        }
        APP_LOG_INFO("WebSocket disconnected for user {}", ctx->user_id);
    }
}

void NotificationController::sendBanNotification(uint64_t user_id,
                                                   const std::string& ban_reason,
                                                   int duration_hours) {
    Json::Value msg;
    msg["type"] = "ban";
    msg["ban_reason"] = ban_reason.empty() ? "违反平台规则" : ban_reason;
    msg["duration_hours"] = duration_hours;
    msg["banned_at"] = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    std::string payload = msg.toStyledString();

    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(user_id);
    if (it != connections_.end()) {
        for (const auto& conn : it->second) {
            conn->send(payload);
        }
    }
}
