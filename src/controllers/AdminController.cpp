#include "AdminController.h"

std::shared_ptr<EmailSender> AdminController::email_sender_ = nullptr;

void AdminController::setEmailSender(std::shared_ptr<EmailSender> sender) {
    email_sender_ = sender;
}

void AdminController::getStats(const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        auto conn = MySQLClient::instance().acquire();

        auto user_count_stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM users WHERE status = 'active' AND role = 'user'");
        auto ucr = user_count_stmt->executeQuery();
        ucr->next();
        int registered_users = ucr->getInt(1);

        long long online_users = RedisClient::instance().scard("online_users");

        auto chat_stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM chat_messages WHERE role = 'user'");
        auto ccr = chat_stmt->executeQuery();
        ccr->next();
        long long total_chats = ccr->getInt(1);

        MySQLClient::instance().release(std::move(conn));

        Json::Value data;
        data["registered_users"] = registered_users;
        data["online_users"] = static_cast<int>(online_users);
        data["total_chats"] = static_cast<int64_t>(total_chats);

        Json::Value result;
        result["code"] = 200;
        result["message"] = "操作成功";
        result["data"] = data;

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(result.toStyledString());
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Get stats error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

void AdminController::getUsers(const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& callback) {
    int page = 1;
    int page_size = 20;
    auto param = req->getParameter("page");
    if (!param.empty()) page = std::stoi(param);
    param = req->getParameter("page_size");
    if (!param.empty()) page_size = std::stoi(param);

    int offset = (page - 1) * page_size;

    try {
        auto conn = MySQLClient::instance().acquire();

        auto count_stmt = conn->prepareStatement(
            "SELECT COUNT(*) FROM users WHERE role = 'user'");
        auto count_res = count_stmt->executeQuery();
        count_res->next();
        int total = count_res->getInt(1);

        auto stmt = conn->prepareStatement(
            "SELECT id, username, email, status, total_chats, "
            "created_at, ban_expires_at, ban_reason FROM users WHERE role = 'user' "
            "ORDER BY created_at DESC LIMIT ? OFFSET ?");
        stmt->setInt(1, page_size);
        stmt->setInt(2, offset);
        auto res = stmt->executeQuery();

        Json::Value users(Json::arrayValue);
        while (res->next()) {
            Json::Value user;
            user["id"] = res->getUInt64("id");
            user["username"] = std::string(res->getString("username"));
            user["email"] = std::string(res->getString("email"));
            user["status"] = std::string(res->getString("status"));
            user["total_chats"] = res->getUInt("total_chats");
            user["created_at"] = std::string(res->getString("created_at"));
            try { user["ban_expires_at"] = std::string(res->getString("ban_expires_at")); } catch (...) { user["ban_expires_at"] = ""; }
            try { user["ban_reason"] = std::string(res->getString("ban_reason")); } catch (...) { user["ban_reason"] = ""; }
            user["online"] = RedisClient::instance().sismember("online_users", std::to_string(res->getUInt64("id")));
            users.append(user);
        }

        MySQLClient::instance().release(std::move(conn));

        Json::Value result;
        result["code"] = 200;
        result["message"] = "操作成功";
        result["data"]["users"] = users;
        result["data"]["total"] = total;
        result["data"]["page"] = page;
        result["data"]["page_size"] = page_size;

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(result.toStyledString());
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Get users error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

void AdminController::searchUsers(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请求格式无效"));
        callback(resp);
        return;
    }

    std::string keyword = (*json)["keyword"].asString();
    std::string search_type = (*json)["type"].asString();

    if (keyword.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请输入搜索关键词"));
        callback(resp);
        return;
    }

    try {
        auto conn = MySQLClient::instance().acquire();

        std::string query;
        std::unique_ptr<sql::PreparedStatement> stmt;

        if (search_type == "username") {
            query = "SELECT id, username, email, status, total_chats, "
                    "created_at, ban_expires_at, ban_reason FROM users WHERE username LIKE ? AND role = 'user' LIMIT 50";
            stmt.reset(conn->prepareStatement(query));
            stmt->setString(1, keyword + "%");
        } else if (search_type == "email") {
            query = "SELECT id, username, email, status, total_chats, "
                    "created_at, ban_expires_at, ban_reason FROM users WHERE email LIKE ? AND role = 'user' LIMIT 50";
            stmt.reset(conn->prepareStatement(query));
            stmt->setString(1, keyword + "%");
        } else if (search_type == "id") {
            query = "SELECT id, username, email, status, total_chats, "
                    "created_at, ban_expires_at, ban_reason FROM users WHERE id = ? AND role = 'user' LIMIT 50";
            stmt.reset(conn->prepareStatement(query));
            stmt->setUInt64(1, std::stoull(keyword));
        } else {
            query = "SELECT id, username, email, status, total_chats, "
                    "created_at, ban_expires_at, ban_reason FROM users WHERE (username LIKE CONCAT(?, '%') OR email LIKE CONCAT(?, '%')) "
                    "AND role = 'user' LIMIT 50";
            stmt.reset(conn->prepareStatement(query));
            stmt->setString(1, keyword);
            stmt->setString(2, keyword);
        }

        auto res = stmt->executeQuery();

        Json::Value users(Json::arrayValue);
        while (res->next()) {
            Json::Value user;
            user["id"] = res->getUInt64("id");
            user["username"] = std::string(res->getString("username"));
            user["email"] = std::string(res->getString("email"));
            user["status"] = std::string(res->getString("status"));
            user["total_chats"] = res->getUInt("total_chats");
            user["created_at"] = std::string(res->getString("created_at"));
            try { user["ban_expires_at"] = std::string(res->getString("ban_expires_at")); } catch (...) { user["ban_expires_at"] = ""; }
            try { user["ban_reason"] = std::string(res->getString("ban_reason")); } catch (...) { user["ban_reason"] = ""; }
            user["online"] = RedisClient::instance().sismember("online_users", std::to_string(res->getUInt64("id")));
            users.append(user);
        }

        MySQLClient::instance().release(std::move(conn));

        Json::Value result;
        result["code"] = 200;
        result["message"] = "操作成功";
        result["data"]["users"] = users;

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(result.toStyledString());
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Search users error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

void AdminController::banUser(const HttpRequestPtr& req,
                               std::function<void(const HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请求格式无效"));
        callback(resp);
        return;
    }

    uint64_t user_id = (*json)["user_id"].asUInt64();
    std::string action = (*json)["action"].asString();

    if (action != "ban" && action != "unban") {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "操作必须是 ban 或 unban"));
        callback(resp);
        return;
    }

    int duration_hours = 0;
    std::string ban_reason;

    if (action == "ban") {
        duration_hours = (*json)["duration_hours"].asInt();
        ban_reason = (*json)["ban_reason"].asString();

        if (duration_hours <= 0) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "请设置封禁时长"));
            callback(resp);
            return;
        }

        if (ban_reason.length() > 50) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "封禁理由不能超过50字"));
            callback(resp);
            return;
        }
    }

    try {
        auto conn = MySQLClient::instance().acquire();

        if (action == "ban") {
            auto stmt = conn->prepareStatement(
                "UPDATE users SET status = 'banned', ban_expires_at = DATE_ADD(NOW(), INTERVAL ? HOUR), "
                "ban_reason = ? WHERE id = ? AND role = 'user'");
            stmt->setInt(1, duration_hours);
            stmt->setString(2, ban_reason);
            stmt->setUInt64(3, user_id);
            int affected = stmt->executeUpdate();

            MySQLClient::instance().release(std::move(conn));

            if (affected == 0) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(404, "用户不存在或是管理员账号"));
                callback(resp);
                return;
            }

            RedisClient::instance().srem("online_users", std::to_string(user_id));
            RedisClient::instance().sadd("banned_users", std::to_string(user_id));

            Json::Value ban_info;
            ban_info["ban_reason"] = ban_reason;
            ban_info["duration_hours"] = duration_hours;
            ban_info["banned_at"] = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            RedisClient::instance().setex("banned_info:" + std::to_string(user_id), duration_hours * 3600,
                                           ban_info.toStyledString());

            NotificationController::sendBanNotification(user_id, ban_reason, duration_hours);

            auto session_key = "user_sessions:" + std::to_string(user_id);
            auto tokens = RedisClient::instance().smembers(session_key);
            std::vector<std::string> keys;
            keys.reserve(tokens.size());
            for (const auto& t : tokens) {
                keys.push_back("session:" + t);
            }
            RedisClient::instance().del_batch(keys);
            RedisClient::instance().del(session_key);

            APP_LOG_INFO("用户 {} 已被封禁 {} 小时，理由: {}", user_id, duration_hours, ban_reason);
        } else {
            auto stmt = conn->prepareStatement(
                "UPDATE users SET status = 'active', ban_expires_at = NULL, "
                "ban_reason = NULL WHERE id = ? AND role = 'user'");
            stmt->setUInt64(1, user_id);
            int affected = stmt->executeUpdate();

            MySQLClient::instance().release(std::move(conn));

            if (affected == 0) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(404, "用户不存在或是管理员账号"));
                callback(resp);
                return;
            }

            RedisClient::instance().srem("banned_users", std::to_string(user_id));
            RedisClient::instance().del("banned_info:" + std::to_string(user_id));
            APP_LOG_INFO("用户 {} 已被解封", user_id);
        }

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateSuccess(action == "ban" ? "用户已被封禁" : "用户已被解封"));
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Ban user error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

void AdminController::deleteUser(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请求格式无效"));
        callback(resp);
        return;
    }

    uint64_t user_id = (*json)["user_id"].asUInt64();

    try {
        auto conn = MySQLClient::instance().acquire();

        auto stmt = conn->prepareStatement(
            "SELECT email, username FROM users WHERE id = ? AND role = 'user'");
        stmt->setUInt64(1, user_id);
        auto res = stmt->executeQuery();

        if (!res->next()) {
            MySQLClient::instance().release(std::move(conn));
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(404, "用户不存在或是管理员账号"));
            callback(resp);
            return;
        }

        std::string email = std::string(res->getString("email"));
        std::string username = std::string(res->getString("username"));

        auto del = conn->prepareStatement("DELETE FROM users WHERE id = ?");
        del->setUInt64(1, user_id);
        del->executeUpdate();
        MySQLClient::instance().release(std::move(conn));

        RedisClient::instance().srem("online_users", std::to_string(user_id));

        auto session_key = "user_sessions:" + std::to_string(user_id);
        auto tokens = RedisClient::instance().smembers(session_key);
        std::vector<std::string> keys;
        keys.reserve(tokens.size());
        for (const auto& t : tokens) {
            keys.push_back("session:" + t);
        }
        RedisClient::instance().del_batch(keys);
        RedisClient::instance().del(session_key);

        APP_LOG_INFO("管理员已强制注销用户 {} ({})", user_id, username);

        if (email_sender_) {
            email_sender_->send_account_deleted_notification(email, "管理员");
            APP_LOG_INFO("已发送注销通知邮件至 {}", email);
        }

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateSuccess("用户已被强制注销"));
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Delete user error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

void AdminController::getLogs(const HttpRequestPtr& req,
                               std::function<void(const HttpResponsePtr&)>&& callback) {
    int page = 1;
    int page_size = 50;
    std::string filter_level;

    auto param = req->getParameter("page");
    if (!param.empty()) page = std::stoi(param);
    param = req->getParameter("page_size");
    if (!param.empty()) page_size = std::stoi(param);
    filter_level = req->getParameter("level");

    std::string log_file = "logs/server.log";
    std::ifstream file(log_file);
    if (!file.is_open()) {
        log_file = "./logs/server.log";
        file.open(log_file);
    }
    if (!file.is_open()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(404, "日志文件不存在"));
        callback(resp);
        return;
    }

    try {
        // 逐行读取全部日志
        std::vector<std::string> all_lines;
        std::string line;
        while (std::getline(file, line)) {
            all_lines.push_back(std::move(line));
        }
        file.close();

        int total_needed = page * page_size;
        if (static_cast<int>(all_lines.size()) > total_needed) {
            all_lines.erase(all_lines.begin(),
                            all_lines.begin() + (all_lines.size() - total_needed));
        }

        if (!filter_level.empty()) {
            std::vector<std::string> filtered;
            filtered.reserve(all_lines.size());
            std::string level_tag = "[" + filter_level + "]";
            for (const auto& line : all_lines) {
                if (line.find(level_tag) != std::string::npos) {
                    filtered.push_back(line);
                }
            }
            all_lines = std::move(filtered);
        }

        int total = static_cast<int>(all_lines.size());
        int offset = (page - 1) * page_size;
        int end = std::min(offset + page_size, total);

        Json::Value logs(Json::arrayValue);
        for (int i = offset; i < end; ++i) {
            Json::Value log;
            log["id"] = i + 1;
            log["message"] = all_lines[total - 1 - i];
            log["level"] = "info";
            log["file"] = "server.log";
            log["line"] = total - i;
            log["created_at"] = "";
            logs.append(log);
        }

        Json::Value result;
        result["code"] = 200;
        result["message"] = "操作成功";
        result["data"]["logs"] = logs;
        result["data"]["total"] = total;
        result["data"]["page"] = page;
        result["data"]["page_size"] = page_size;

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(result.toStyledString());
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Get logs error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

std::string AdminController::generateError(int code, const std::string& message) {
    Json::Value result;
    result["code"] = code;
    result["message"] = message;
    return result.toStyledString();
}

std::string AdminController::generateSuccess(const std::string& message) {
    Json::Value result;
    result["code"] = 200;
    result["message"] = message;
    return result.toStyledString();
}
