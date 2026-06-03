#include "UserController.h"

// getUserIdFromToken
// 功能：从 HTTP 请求的 Authorization 头中解析 JWT token 并提取用户 ID
// 参数：req - HTTP 请求对象
// 返回值：uint64_t - 用户 ID
uint64_t UserController::getUserIdFromToken(const HttpRequestPtr& req) {
    auto auth = req->getHeader("Authorization");
    if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);
    auto data = JWTUtils::verify_token(auth);
    return data.user_id;
}

// getProfile
// 功能：获取当前登录用户的个人信息
// 参数：req - HTTP 请求对象；callback - 异步响应回调
void UserController::getProfile(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        uint64_t user_id = getUserIdFromToken(req);
        auto conn = MySQLClient::instance().acquire();

        auto stmt = conn->prepareStatement(
            "SELECT id, username, email, role, total_chats, "
            "created_at FROM users WHERE id = ? AND status != 'deleted'");
        stmt->setUInt64(1, user_id);
        auto res = stmt->executeQuery();

        if (!res->next()) {
            MySQLClient::instance().release(std::move(conn));
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(404, "用户不存在"));
            callback(resp);
            return;
        }

        Json::Value data;
        data["id"] = res->getUInt64("id");
        data["username"] = std::string(res->getString("username"));
        data["email"] = std::string(res->getString("email"));
        data["role"] = std::string(res->getString("role"));
        data["total_chats"] = res->getUInt("total_chats");
        data["created_at"] = std::string(res->getString("created_at"));

        MySQLClient::instance().release(std::move(conn));

        Json::Value result;
        result["code"] = 200;
        result["message"] = "操作成功";
        result["data"] = data;

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(result.toStyledString());
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Get profile error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

// updateProfile
// 功能：更新当前登录用户的个人信息（用户名或昵称）
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：支持单独更新用户名或昵称，也可同时更新两者
void UserController::updateProfile(const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请求格式无效"));
        callback(resp);
        return;
    }

    uint64_t user_id = getUserIdFromToken(req);
    std::string new_username = (*json)["username"].asString();
    std::string new_nickname = (*json)["nickname"].asString();

    try {
        auto conn = MySQLClient::instance().acquire();

        // 如果修改用户名，检查新用户名是否已被其他用户使用
        if (!new_username.empty()) {
            auto check = conn->prepareStatement(
                "SELECT COUNT(*) FROM users WHERE username = ? AND id != ?");
            check->setString(1, new_username);
            check->setUInt64(2, user_id);
            auto check_res = check->executeQuery();
            check_res->next();
            if (check_res->getInt(1) > 0) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(409, "用户名已被使用"));
                callback(resp);
                return;
            }
        }

        // 根据提供的字段组合不同的更新语句
        if (!new_username.empty() && !new_nickname.empty()) {
            auto stmt = conn->prepareStatement(
                "UPDATE users SET username = ?, nickname = ? WHERE id = ?");
            stmt->setString(1, new_username);
            stmt->setString(2, new_nickname);
            stmt->setUInt64(3, user_id);
            stmt->executeUpdate();
        } else if (!new_username.empty()) {
            auto stmt = conn->prepareStatement(
                "UPDATE users SET username = ? WHERE id = ?");
            stmt->setString(1, new_username);
            stmt->setUInt64(2, user_id);
            stmt->executeUpdate();
        } else if (!new_nickname.empty()) {
            auto stmt = conn->prepareStatement(
                "UPDATE users SET nickname = ? WHERE id = ?");
            stmt->setString(1, new_nickname);
            stmt->setUInt64(2, user_id);
            stmt->executeUpdate();
        }

        MySQLClient::instance().release(std::move(conn));

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateSuccess("个人信息已更新"));
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Update profile error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

// changePassword
// 功能：修改当前登录用户的密码
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：需要验证旧密码，修改成功后清除所有会话强制重新登录
void UserController::changePassword(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback) {
    // IP 级别频率限制，5 分钟内最多 10 次
    std::string rate_key = "rate_limit:" + std::string(__func__) + ":" + req->getPeerAddr().toIp();
    auto attempts = RedisClient::instance().incr(rate_key);
    if (attempts == 1) RedisClient::instance().expire(rate_key, 300);
    if (attempts > 10) { auto resp = HttpResponse::newHttpResponse(); resp->setBody(generateError(429, "操作过于频繁，请稍后再试")); callback(resp); return; }
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请求格式无效"));
        callback(resp);
        return;
    }

    uint64_t user_id = getUserIdFromToken(req);
    std::string old_password = (*json)["old_password"].asString();
    std::string new_password = (*json)["new_password"].asString();

    try {
        auto conn = MySQLClient::instance().acquire();

        auto stmt = conn->prepareStatement(
            "SELECT password_hash, password_salt FROM users WHERE id = ?");
        stmt->setUInt64(1, user_id);
        auto res = stmt->executeQuery();

        if (!res->next()) {
            MySQLClient::instance().release(std::move(conn));
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(404, "用户不存在"));
            callback(resp);
            return;
        }

        std::string old_hash = std::string(res->getString("password_hash"));
        std::string old_salt = std::string(res->getString("password_salt"));

        // 校验旧密码
        if (!PasswordHasher::verify_password(old_password, old_salt, old_hash)) {
            MySQLClient::instance().release(std::move(conn));
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "旧密码错误"));
            callback(resp);
            return;
        }

        // 生成新密码哈希
        std::string new_hash, new_salt;
        PasswordHasher::generate_hash_and_salt(new_password, new_hash, new_salt);

        auto update = conn->prepareStatement(
            "UPDATE users SET password_hash = ?, password_salt = ? WHERE id = ?");
        update->setString(1, new_hash);
        update->setString(2, new_salt);
        update->setUInt64(3, user_id);
        update->executeUpdate();

        MySQLClient::instance().release(std::move(conn));

        // 清除所有会话，强制用户重新登录
        auto session_key = "user_sessions:" + std::to_string(user_id);
        auto tokens = RedisClient::instance().smembers(session_key);
        std::vector<std::string> keys;
        keys.reserve(tokens.size());
        for (const auto& t : tokens) {
            keys.push_back("session:" + t);
        }
        RedisClient::instance().del_batch(keys);
        RedisClient::instance().del(session_key);
        RedisClient::instance().srem("online_users", std::to_string(user_id));

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateSuccess("密码修改成功"));
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Change password error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

// deleteAccount
// 功能：提交账号注销申请
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：需要邮箱验证码，提交后将账号状态设为 pending_deletion，进入 24 小时冷静期
void UserController::deleteAccount(const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string rate_key = "rate_limit:" + std::string(__func__) + ":" + req->getPeerAddr().toIp();
    auto attempts = RedisClient::instance().incr(rate_key);
    if (attempts == 1) RedisClient::instance().expire(rate_key, 300);
    if (attempts > 10) { auto resp = HttpResponse::newHttpResponse(); resp->setBody(generateError(429, "操作过于频繁，请稍后再试")); callback(resp); return; }
    auto json = req->getJsonObject();
    if (!json || !(*json)["code"].isString()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "请提供验证码"));
        callback(resp);
        return;
    }

    try {
        uint64_t user_id = getUserIdFromToken(req);
        auto auth = req->getHeader("Authorization");
        if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);

        auto conn = MySQLClient::instance().acquire();
        auto stmt = conn->prepareStatement(
            "SELECT email, cancel_delete_until FROM users WHERE id = ?");
        stmt->setUInt64(1, user_id);
        auto res = stmt->executeQuery();

        if (!res->next()) {
            MySQLClient::instance().release(std::move(conn));
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(404, "用户不存在"));
            callback(resp);
            return;
        }
        std::string email = std::string(res->getString("email"));

        // 检查冷静期内是否取消过注销（72 小时内不可再次申请）
        std::string cancel_delete_until_str;
        try { cancel_delete_until_str = std::string(res->getString("cancel_delete_until")); } catch (...) {}
        if (!cancel_delete_until_str.empty()) {
            bool cooldown_active = false;
            try {
                std::tm tm = {};
                std::stringstream ss(cancel_delete_until_str);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    auto cancel_until_tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    auto now = std::chrono::system_clock::now();
                    if (now < cancel_until_tp) {
                        cooldown_active = true;
                    }
                }
            } catch (...) {}
            if (cooldown_active) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(400, "您曾取消过注销申请，72小时内不允许再次申请注销，请稍后再试"));
                callback(resp);
                return;
            }
        }
        MySQLClient::instance().release(std::move(conn));

        // 校验验证码
        std::string redis_key = std::string("verify_code:") + email + ":delete_account";
        std::string stored_code = RedisClient::instance().get(redis_key);
        std::string input_code = (*json)["code"].asString();

        if (stored_code.empty() || stored_code != input_code) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "验证码无效或已过期"));
            callback(resp);
            return;
        }
        RedisClient::instance().del(redis_key);

        // 设置账号为 pending_deletion 状态，24 小时后自动删除
        conn = MySQLClient::instance().acquire();
        auto upd = conn->prepareStatement(
            "UPDATE users SET status = 'pending_deletion', deleted_at = DATE_ADD(NOW(), INTERVAL 24 HOUR), "
            "cancel_delete_until = NULL WHERE id = ?");
        upd->setUInt64(1, user_id);
        upd->executeUpdate();
        MySQLClient::instance().release(std::move(conn));

        // 清除当前会话和在线状态
        RedisClient::instance().del("session:" + auth);
        RedisClient::instance().srem("online_users", std::to_string(user_id));

        // 清除该用户的所有其他会话
        auto session_key = "user_sessions:" + std::to_string(user_id);
        auto tokens = RedisClient::instance().smembers(session_key);
        std::vector<std::string> keys;
        keys.reserve(tokens.size());
        for (const auto& t : tokens) {
            keys.push_back("session:" + t);
        }
        RedisClient::instance().del_batch(keys);
        RedisClient::instance().del(session_key);

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateSuccess("注销申请已提交，24小时冷静期后将自动删除账号"));
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Delete account error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

// cancelDelete
// 功能：取消账号注销申请
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：支持已登录和未登录两种取消方式，取消后 72 小时内不可再次申请注销
void UserController::cancelDelete(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        auto auth = req->getHeader("Authorization");
        uint64_t user_id = 0;

        // 优先尝试从 token 获取用户 ID
        if (!auth.empty()) {
            auto token = auth;
            if (token.substr(0, 7) == "Bearer ") token = token.substr(7);
            try {
                auto data = JWTUtils::verify_token(token);
                user_id = data.user_id;
            } catch (...) {}
        }

        // 如果没有有效的 token，则通过用户名和密码验证身份
        if (user_id == 0) {
            auto json = req->getJsonObject();
            if (!json || !(*json)["username"].isString() || !(*json)["password"].isString()) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(400, "请提供有效的身份认证"));
                callback(resp);
                return;
            }

            std::string credential = (*json)["username"].asString();
            std::string password = (*json)["password"].asString();
            bool is_email = credential.find('@') != std::string::npos;

            auto conn = MySQLClient::instance().acquire();
            auto stmt = conn->prepareStatement(
                "SELECT id, password_hash, password_salt FROM users WHERE " +
                std::string(is_email ? "email = ?" : "username = ?") +
                " AND status = 'pending_deletion'");
            stmt->setString(1, credential);
            auto res = stmt->executeQuery();

            if (!res->next()) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(401, "用户不存在或未处于注销冷静期"));
                callback(resp);
                return;
            }

            std::string password_hash = std::string(res->getString("password_hash"));
            std::string password_salt = std::string(res->getString("password_salt"));

            if (!PasswordHasher::verify_password(password, password_salt, password_hash)) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(401, "密码错误"));
                callback(resp);
                return;
            }

            user_id = res->getUInt64("id");
            MySQLClient::instance().release(std::move(conn));
        }

        // 恢复账号为活跃状态，设置 cancel_delete_until（3 天内不可再次申请注销）
        auto conn = MySQLClient::instance().acquire();
        auto upd = conn->prepareStatement(
            "UPDATE users SET status = 'active', deleted_at = NULL, "
            "cancel_delete_until = DATE_ADD(NOW(), INTERVAL 3 DAY) WHERE id = ? AND status = 'pending_deletion'");
        upd->setUInt64(1, user_id);
        upd->executeUpdate();
        MySQLClient::instance().release(std::move(conn));

        RedisClient::instance().sadd("online_users", std::to_string(user_id));

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateSuccess("注销已取消，账号恢复正常"));
        callback(resp);

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Cancel delete error: {}", e.what());
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(500, "服务器错误"));
        callback(resp);
    }
}

// generateError
// 功能：生成错误响应 JSON 字符串
// 参数：code - 错误状态码；message - 错误信息
// 返回值：string - JSON 格式的错误响应字符串
std::string UserController::generateError(int code, const std::string& message) {
    Json::Value result;
    result["code"] = code;
    result["message"] = message;
    return result.toStyledString();
}

// generateSuccess
// 功能：生成成功响应 JSON 字符串
// 参数：message - 成功信息
// 返回值：string - JSON 格式的成功响应字符串
std::string UserController::generateSuccess(const std::string& message) {
    Json::Value result;
    result["code"] = 200;
    result["message"] = message;
    return result.toStyledString();
}
