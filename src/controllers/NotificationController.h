#pragma once

#include <drogon/WebSocketController.h>
#include <drogon/drogon.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>
#include <chrono>
#include "utils/JWTUtils.h"
#include "database/RedisClient.h"
#include "utils/Logger.h"

using namespace drogon;

class NotificationController : public WebSocketController<NotificationController> {
public:
    void handleNewConnection(const HttpRequestPtr& req,
                              const WebSocketConnectionPtr& conn) override;

    void handleNewMessage(const WebSocketConnectionPtr& conn,
                           std::string&& message,
                           const WebSocketMessageType& type) override;

    void handleConnectionClosed(const WebSocketConnectionPtr& conn) override;

    static void sendBanNotification(uint64_t user_id,
                                     const std::string& ban_reason,
                                     int duration_hours);

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/notification");
    WS_PATH_LIST_END

private:
    struct UserContext {
        uint64_t user_id;
        std::string username;
        UserContext(uint64_t uid, const std::string& uname)
            : user_id(uid), username(uname) {}
    };

    static inline std::mutex connections_mutex_;
    static inline std::unordered_map<uint64_t, std::vector<WebSocketConnectionPtr>> connections_;
};
