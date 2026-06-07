#include "NotificationController.h"
#include "utils/JWTUtils.h"
#include "database/RedisClient.h"
#include "utils/Logger.h"
#include <algorithm>

using namespace drogon;

// handleNewConnection
// 功能：处理新的 WebSocket 连接请求，进行 token 认证并建立连接映射
// 参数：req - HTTP 请求对象；conn - WebSocket 连接对象
// 说明：从请求参数或 Authorization 头中提取 token 进行验证，验证通过后记录用户连接
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
        // 验证会话是否有效
        if (!RedisClient::instance().exists(session_key)) {
            conn->shutdown();
            return;
        }

        conn->setContext(std::make_shared<UserContext>(user_id, data.username));

        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            // 将连接添加到对应用户的连接列表中
            connections_[user_id].push_back(conn);
        }

        APP_LOG_INFO("WebSocket connected for user {} ({})", user_id, data.username);
    } catch (const std::exception& e) {
        APP_LOG_WARN("WebSocket auth failed: {}", e.what());
        conn->shutdown();
    }
}

// handleNewMessage
// 功能：处理 WebSocket 新消息（当前为空实现）
// 参数：conn - WebSocket 连接对象；message - 消息内容；type - 消息类型
void NotificationController::handleNewMessage(const WebSocketConnectionPtr& conn,
                                                std::string&& message,
                                                const WebSocketMessageType& type) {}

// handleConnectionClosed
// 功能：处理 WebSocket 连接关闭事件，清理连接映射
// 参数：conn - 被关闭的 WebSocket 连接对象
void NotificationController::handleConnectionClosed(const WebSocketConnectionPtr& conn) {
    auto ctx = conn->getContext<UserContext>();
    if (ctx) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(ctx->user_id);
        if (it != connections_.end()) {
            auto& vec = it->second;
            // 从连接列表中移除当前连接
            vec.erase(std::remove(vec.begin(), vec.end(), conn), vec.end());
            if (vec.empty()) {
                // 如果用户没有其他连接，删除整个条目
                connections_.erase(it);
            }
        }
        APP_LOG_INFO("WebSocket disconnected for user {}", ctx->user_id);
    }
}

// sendBanNotification
// 功能：向指定用户发送封禁通知的 WebSocket 消息
// 参数：user_id - 目标用户 ID；ban_reason - 封禁原因；duration_hours - 封禁时长（小时）
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
        // 向用户的所有活跃连接发送通知
        for (const auto& conn : it->second) {
            conn->send(payload);
        }
    }
}


