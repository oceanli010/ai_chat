#pragma once

#include <drogon/WebSocketController.h>
#include <drogon/drogon.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

// NotificationController
// 功能：处理 WebSocket 连接相关的通知推送业务，包括用户封禁通知
class NotificationController : public drogon::WebSocketController<NotificationController> {
public:
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                              const drogon::WebSocketConnectionPtr& conn) override;
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                           std::string&& message,
                           const drogon::WebSocketMessageType& type) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    // sendBanNotification
    // 功能：向指定用户发送封禁通知
    // 参数：user_id - 用户 ID；ban_reason - 封禁原因；duration_hours - 封禁时长（小时）
    static void sendBanNotification(uint64_t user_id,
                                     const std::string& ban_reason,
                                     int duration_hours);

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/notification");
    WS_PATH_LIST_END

private:
    // UserContext
    // 功能：存储 WebSocket 连接关联的用户上下文信息
    struct UserContext {
        uint64_t user_id;     // 用户 ID
        std::string username; // 用户名
        UserContext(uint64_t uid, const std::string& uname)
            : user_id(uid), username(uname) {}
    };

    static inline std::mutex connections_mutex_;                                           // 连接集合的互斥锁
    static inline std::unordered_map<uint64_t, std::vector<drogon::WebSocketConnectionPtr>> connections_; // 用户 ID 到 WebSocket 连接列表的映射
};
