#pragma once

#include <drogon/WebSocketController.h>
#include <drogon/drogon.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

class NotificationController : public drogon::WebSocketController<NotificationController> {
public:
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                              const drogon::WebSocketConnectionPtr& conn) override;
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                           std::string&& message,
                           const drogon::WebSocketMessageType& type) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    static void sendBanNotification(uint64_t user_id,
                                     const std::string& ban_reason,
                                     int duration_hours);
    static void sendAnnouncement(const std::string& title,
                                  const std::string& content,
                                  uint64_t announcement_id,
                                  const std::string& level = "normal");

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
    static inline std::unordered_map<uint64_t, std::vector<drogon::WebSocketConnectionPtr>> connections_;
};
