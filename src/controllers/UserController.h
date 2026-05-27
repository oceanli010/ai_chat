#pragma once

#include <drogon/drogon.h>
#include <drogon/HttpController.h>
#include <string>
#include "database/MySQLClient.h"
#include "database/RedisClient.h"
#include "utils/PasswordHasher.h"
#include "utils/JWTUtils.h"
#include "utils/Logger.h"

using namespace drogon;

class UserController : public HttpController<UserController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::getProfile, "/api/user/profile", Get);
    ADD_METHOD_TO(UserController::updateProfile, "/api/user/profile", Put);
    ADD_METHOD_TO(UserController::changePassword, "/api/user/password", Put);
    ADD_METHOD_TO(UserController::deleteAccount, "/api/user/account", Delete);
    ADD_METHOD_TO(UserController::cancelDelete, "/api/user/cancel-delete", Post);
    METHOD_LIST_END

    uint64_t getUserIdFromToken(const HttpRequestPtr& req) {
        auto auth = req->getHeader("Authorization");
        if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);
        auto data = JWTUtils::verify_token(auth);
        return data.user_id;
    }

    void getProfile(const HttpRequestPtr& req,
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

    void updateProfile(const HttpRequestPtr& req,
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

    void changePassword(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback) {
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

            if (!PasswordHasher::verify_password(old_password, old_salt, old_hash)) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(400, "旧密码错误"));
                callback(resp);
                return;
            }

            std::string new_hash, new_salt;
            PasswordHasher::generate_hash_and_salt(new_password, new_hash, new_salt);

            auto update = conn->prepareStatement(
                "UPDATE users SET password_hash = ?, password_salt = ? WHERE id = ?");
            update->setString(1, new_hash);
            update->setString(2, new_salt);
            update->setUInt64(3, user_id);
            update->executeUpdate();

            MySQLClient::instance().release(std::move(conn));

            // 密码修改成功后，使所有旧session失效
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

    void deleteAccount(const HttpRequestPtr& req,
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

            std::string cancel_delete_until_str;
            try { cancel_delete_until_str = std::string(res->getString("cancel_delete_until")); } catch (...) {}
            if (!cancel_delete_until_str.empty()) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(400, "您曾取消过注销申请，72小时内不允许再次申请注销，请稍后再试"));
                callback(resp);
                return;
            }
            MySQLClient::instance().release(std::move(conn));

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

            conn = MySQLClient::instance().acquire();
            auto upd = conn->prepareStatement(
                "UPDATE users SET status = 'pending_deletion', deleted_at = DATE_ADD(NOW(), INTERVAL 24 HOUR) WHERE id = ?");
            upd->setUInt64(1, user_id);
            upd->executeUpdate();
            MySQLClient::instance().release(std::move(conn));

            RedisClient::instance().del("session:" + auth);
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

    void cancelDelete(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
        try {
            auto auth = req->getHeader("Authorization");
            uint64_t user_id = 0;

            if (!auth.empty()) {
                auto token = auth;
                if (token.substr(0, 7) == "Bearer ") token = token.substr(7);
                try {
                    auto data = JWTUtils::verify_token(token);
                    user_id = data.user_id;
                } catch (...) {}
            }

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

private:
    static std::string generateError(int code, const std::string& message) {
        Json::Value result;
        result["code"] = code;
        result["message"] = message;
        return result.toStyledString();
    }

    static std::string generateSuccess(const std::string& message) {
        Json::Value result;
        result["code"] = 200;
        result["message"] = message;
        return result.toStyledString();
    }
};
