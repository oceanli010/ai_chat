#pragma once

#include <drogon/HttpController.h>
#include <functional>
#include <string>

class AnnouncementController : public drogon::HttpController<AnnouncementController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AnnouncementController::create, "/api/admin/announcements", drogon::Post);
    ADD_METHOD_TO(AnnouncementController::list, "/api/announcements", drogon::Get);
    ADD_METHOD_TO(AnnouncementController::detail, "/api/announcements/{id}", drogon::Get);
    ADD_METHOD_TO(AnnouncementController::markRead, "/api/announcements/{id}/read", drogon::Put);
    ADD_METHOD_TO(AnnouncementController::remove, "/api/admin/announcements/{id}", drogon::Delete);
    ADD_METHOD_TO(AnnouncementController::batchDelete, "/api/admin/announcements/batch-delete", drogon::Post);
    ADD_METHOD_TO(AnnouncementController::readStats, "/api/admin/announcements/{id}/stats", drogon::Get);
    ADD_METHOD_TO(AnnouncementController::unreadCount, "/api/announcements/unread-count", drogon::Get);
    METHOD_LIST_END

    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void detail(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                uint64_t announcement_id);
    void markRead(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  uint64_t announcement_id);
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                uint64_t announcement_id);
    void batchDelete(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void readStats(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   uint64_t announcement_id);
    void unreadCount(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
