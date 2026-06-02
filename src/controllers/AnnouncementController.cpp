#include "AnnouncementController.h"
#include "NotificationController.h"
#include "database/MySQLClient.h"
#include "utils/JWTUtils.h"
#include "utils/Logger.h"
#include <drogon/drogon.h>

using namespace drogon;

static uint64_t getUserId(const HttpRequestPtr& req) {
    auto auth = req->getHeader("Authorization");
    if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);
    return JWTUtils::verify_token(auth).user_id;
}

static std::string getRole(const HttpRequestPtr& req) {
    auto auth = req->getHeader("Authorization");
    if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);
    return JWTUtils::verify_token(auth).role;
}

static HttpResponsePtr jsonResponse(int code, const std::string& msg) {
    auto resp = HttpResponse::newHttpResponse();
    Json::Value json;
    json["code"] = code;
    json["message"] = msg;
    resp->setBody(json.toStyledString());
    return resp;
}

static HttpResponsePtr jsonResponse(int code, const std::string& msg, const Json::Value& data) {
    auto resp = HttpResponse::newHttpResponse();
    Json::Value json;
    json["code"] = code;
    json["message"] = msg;
    json["data"] = data;
    resp->setBody(json.toStyledString());
    return resp;
}

void AnnouncementController::create(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback) {
    if (getRole(req) != "admin") {
        callback(jsonResponse(403, "仅管理员可发布公告"));
        return;
    }
    auto json = req->getJsonObject();
    if (!json) {
        callback(jsonResponse(400, "请求格式无效"));
        return;
    }
    std::string title = (*json)["title"].asString();
    std::string content = (*json)["content"].asString();
    std::string level = (*json)["level"].asString();
    bool auto_delete = (*json).get("auto_delete", true).asBool();

    if (title.empty() || title.length() > 100) {
        callback(jsonResponse(400, "标题需1-100字符"));
        return;
    }
    if (content.empty() || content.length() > 5000) {
        callback(jsonResponse(400, "正文需1-5000字符"));
        return;
    }
    if (level != "important" && level != "normal") {
        callback(jsonResponse(400, "等级必须为important或normal"));
        return;
    }

    uint64_t admin_id = getUserId(req);

    try {
        auto conn = MySQLClient::instance().acquire();
        auto stmt = conn->prepareStatement(
            "INSERT INTO announcements (title, content, level, auto_delete, admin_id) VALUES (?, ?, ?, ?, ?)");
        stmt->setString(1, title);
        stmt->setString(2, content);
        stmt->setString(3, level);
        stmt->setBoolean(4, auto_delete);
        stmt->setUInt64(5, admin_id);
        stmt->executeUpdate();

        uint64_t ann_id = 0;
        auto id_stmt = conn->prepareStatement("SELECT LAST_INSERT_ID()");
        auto id_res = id_stmt->executeQuery();
        if (id_res->next()) ann_id = id_res->getUInt64(1);
        conn.reset();

        APP_LOG_INFO("公告已发布: {} (level={}, auto_delete={}, ID={})", title, level, auto_delete, ann_id);

        app().getLoop()->queueInLoop([title, content, ann_id, level]() {
            NotificationController::sendAnnouncement(title, content, ann_id, level);
        });

        Json::Value data;
        data["id"] = ann_id;
        callback(jsonResponse(200, "公告已发布", data));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("Create announcement error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}

void AnnouncementController::list(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    uint64_t user_id = getUserId(req);
    int page = std::stoi(req->getParameter("page").empty() ? "1" : req->getParameter("page"));
    int size = std::stoi(req->getParameter("size").empty() ? "20" : req->getParameter("size"));
    if (page < 1) page = 1;
    if (size < 1 || size > 50) size = 20;
    int offset = (page - 1) * size;

    try {
        auto conn = MySQLClient::instance().acquire();

        auto count_stmt = conn->prepareStatement("SELECT COUNT(*) FROM announcements");
        auto count_res = count_stmt->executeQuery();
        count_res->next();
        int total = count_res->getInt(1);

        auto stmt = conn->prepareStatement(
            "SELECT a.id, a.title, a.content, a.level, a.auto_delete, a.admin_id, a.created_at, "
            "(SELECT id FROM announcement_reads WHERE user_id = ? AND announcement_id = a.id) IS NOT NULL AS is_read "
            "FROM announcements a ORDER BY a.created_at DESC LIMIT ? OFFSET ?");
        stmt->setUInt64(1, user_id);
        stmt->setInt(2, size);
        stmt->setInt(3, offset);
        auto res = stmt->executeQuery();

        Json::Value list_data(Json::arrayValue);
        while (res->next()) {
            uint64_t ann_id = res->getUInt64("id");
            bool is_read = res->getBoolean("is_read");

            Json::Value item;
            item["id"] = ann_id;
            item["title"] = std::string(res->getString("title"));
            item["content"] = std::string(res->getString("content"));
            item["level"] = std::string(res->getString("level"));
            item["auto_delete"] = res->getBoolean("auto_delete");
            item["admin_id"] = res->getUInt64("admin_id");
            item["created_at"] = std::string(res->getString("created_at"));
            item["is_read"] = is_read;
            list_data.append(item);
        }
        conn.reset();

        Json::Value data;
        data["announcements"] = list_data;
        data["total"] = total;
        data["page"] = page;
        data["size"] = size;
        data["total_pages"] = (total + size - 1) / size;
        callback(jsonResponse(200, "操作成功", data));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("List announcements error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}

void AnnouncementController::detail(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback,
                                     uint64_t announcement_id) {
    uint64_t user_id = getUserId(req);
    try {
        auto conn = MySQLClient::instance().acquire();
        auto stmt = conn->prepareStatement(
            "SELECT a.id, a.title, a.content, a.level, a.auto_delete, a.admin_id, a.created_at "
            "FROM announcements a WHERE a.id = ?");
        stmt->setUInt64(1, announcement_id);
        auto res = stmt->executeQuery();
        if (!res->next()) {
            conn.reset();
            callback(jsonResponse(404, "公告不存在"));
            return;
        }

        auto read_stmt = conn->prepareStatement(
            "SELECT id FROM announcement_reads WHERE user_id = ? AND announcement_id = ?");
        read_stmt->setUInt64(1, user_id);
        read_stmt->setUInt64(2, announcement_id);
        auto read_res = read_stmt->executeQuery();
        bool is_read = read_res->next();

        Json::Value data;
        data["id"] = res->getUInt64("id");
        data["title"] = std::string(res->getString("title"));
        data["content"] = std::string(res->getString("content"));
        data["level"] = std::string(res->getString("level"));
        data["auto_delete"] = res->getBoolean("auto_delete");
        data["admin_id"] = res->getUInt64("admin_id");
        data["created_at"] = std::string(res->getString("created_at"));
        data["is_read"] = is_read;
        conn.reset();

        callback(jsonResponse(200, "操作成功", data));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("Get announcement detail error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}

void AnnouncementController::markRead(const HttpRequestPtr& req,
                                       std::function<void(const HttpResponsePtr&)>&& callback,
                                       uint64_t announcement_id) {
    uint64_t user_id = getUserId(req);
    try {
        auto conn = MySQLClient::instance().acquire();
        auto stmt = conn->prepareStatement(
            "INSERT IGNORE INTO announcement_reads (user_id, announcement_id) VALUES (?, ?)");
        stmt->setUInt64(1, user_id);
        stmt->setUInt64(2, announcement_id);
        stmt->executeUpdate();
        conn.reset();
        callback(jsonResponse(200, "已标记为已读"));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("Mark read error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}

void AnnouncementController::remove(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback,
                                     uint64_t announcement_id) {
    if (getRole(req) != "admin") {
        callback(jsonResponse(403, "仅管理员可删除公告"));
        return;
    }
    try {
        auto conn = MySQLClient::instance().acquire();
        auto stmt = conn->prepareStatement("DELETE FROM announcements WHERE id = ?");
        stmt->setUInt64(1, announcement_id);
        stmt->executeUpdate();
        conn.reset();
        callback(jsonResponse(200, "公告已删除"));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("Delete announcement error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}

void AnnouncementController::batchDelete(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback) {
    if (getRole(req) != "admin") {
        callback(jsonResponse(403, "仅管理员可删除公告"));
        return;
    }
    auto json = req->getJsonObject();
    if (!json || !(*json)["ids"].isArray()) {
        callback(jsonResponse(400, "请提供要删除的公告ID列表"));
        return;
    }
    try {
        auto conn = MySQLClient::instance().acquire();
        int count = 0;
        for (const auto& id_val : (*json)["ids"]) {
            auto stmt = conn->prepareStatement("DELETE FROM announcements WHERE id = ?");
            stmt->setUInt64(1, id_val.asUInt64());
            count += stmt->executeUpdate();
        }
        conn.reset();
        callback(jsonResponse(200, "已删除" + std::to_string(count) + "条公告"));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("Batch delete error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}

void AnnouncementController::readStats(const HttpRequestPtr& req,
                                        std::function<void(const HttpResponsePtr&)>&& callback,
                                        uint64_t announcement_id) {
    if (getRole(req) != "admin") {
        callback(jsonResponse(403, "仅管理员可查看统计"));
        return;
    }
    try {
        auto conn = MySQLClient::instance().acquire();
        auto total_stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM users WHERE role = 'user'");
        auto total_res = total_stmt->executeQuery();
        total_res->next();
        int total_users = total_res->getInt(1);

        auto read_stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM announcement_reads WHERE announcement_id = ?");
        read_stmt->setUInt64(1, announcement_id);
        auto read_res = read_stmt->executeQuery();
        read_res->next();
        int read_count = read_res->getInt(1);
        conn.reset();

        Json::Value data;
        data["total_users"] = total_users;
        data["read_count"] = read_count;
        data["unread_count"] = total_users - read_count;
        callback(jsonResponse(200, "操作成功", data));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("Read stats error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}

void AnnouncementController::unreadCount(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback) {
    uint64_t user_id = getUserId(req);
    try {
        auto conn = MySQLClient::instance().acquire();
        auto stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM announcements a WHERE a.id NOT IN "
            "(SELECT announcement_id FROM announcement_reads WHERE user_id = ?)");
        stmt->setUInt64(1, user_id);
        auto res = stmt->executeQuery();
        res->next();
        int count = res->getInt(1);
        conn.reset();

        Json::Value data;
        data["unread_count"] = count;
        callback(jsonResponse(200, "操作成功", data));
    } catch (const std::exception& e) {
        APP_LOG_ERROR("Unread count error: {}", e.what());
        callback(jsonResponse(500, "服务器错误"));
    }
}
