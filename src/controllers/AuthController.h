#pragma once

#include <drogon/drogon.h>
#include <drogon/HttpController.h>
#include <string>
#include <random>
#include <memory>
#include "database/MySQLClient.h"
#include "database/RedisClient.h"
#include "utils/PasswordHasher.h"
#include "utils/IDGenerator.h"
#include "utils/JWTUtils.h"
#include "utils/Validator.h"
#include "utils/EmailSender.h"
#include "utils/Logger.h"

using namespace drogon;

class AuthController : public HttpController<AuthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::sendCode, "/api/auth/send-code", Post);
    ADD_METHOD_TO(AuthController::checkEmail, "/api/auth/check-email", Post);
    ADD_METHOD_TO(AuthController::sendDeleteCode, "/api/auth/send-delete-code", Post);
    ADD_METHOD_TO(AuthController::registerUser, "/api/auth/register", Post);
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", Post);
    ADD_METHOD_TO(AuthController::resetPassword, "/api/auth/reset-password", Post);
    METHOD_LIST_END

    void checkEmail(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "请求格式无效"));
            callback(resp);
            return;
        }

        std::string email = (*json)["email"].asString();
        if (!Validator::is_valid_email(email)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "邮箱格式无效"));
            callback(resp);
            return;
        }

        try {
            auto conn = MySQLClient::instance().acquire();
            auto stmt = conn->prepareStatement(
                "SELECT COUNT(*) FROM users WHERE email = ?");
            stmt->setString(1, email);
            auto res = stmt->executeQuery();
            res->next();
            int count = res->getInt(1);
            MySQLClient::instance().release(std::move(conn));

            Json::Value result;
            result["code"] = 200;
            result["data"]["registered"] = (count > 0);
            result["message"] = "操作成功";
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(result.toStyledString());
            callback(resp);
        } catch (const std::exception& e) {
            APP_LOG_ERROR("Check email error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    void sendCode(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "请求格式无效"));
            callback(resp);
            return;
        }

        std::string email = (*json)["email"].asString();
        std::string type = (*json)["type"].asString();

        if (!Validator::is_valid_email(email)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "邮箱格式无效"));
            callback(resp);
            return;
        }

        std::string code = generateVerificationCode();
        std::string redis_key = std::string("verify_code:") + email + ":" + type;
        RedisClient::instance().setex(redis_key, 300, code);

        bool email_sent = false;
        if (email_sender_) {
            email_sent = email_sender_->send_verification_code(email, code);
        }

        APP_LOG_INFO("Verification code for {}: {} (email sent: {})", email, code, email_sent);

        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateSuccess("验证码已发送"));
        callback(resp);
    }

    void sendDeleteCode(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback) {
        auto auth = req->getHeader("Authorization");
        if (auth.substr(0, 7) == "Bearer ") auth = auth.substr(7);

        try {
            auto data = JWTUtils::verify_token(auth);
            auto conn = MySQLClient::instance().acquire();
            auto stmt = conn->prepareStatement(
                "SELECT email FROM users WHERE id = ?");
            stmt->setUInt64(1, data.user_id);
            auto res = stmt->executeQuery();
            if (!res->next()) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(404, "用户不存在"));
                callback(resp);
                return;
            }
            std::string email = std::string(res->getString("email"));
            MySQLClient::instance().release(std::move(conn));

            std::string code = generateVerificationCode();
            std::string redis_key = std::string("verify_code:") + email + ":delete_account";
            RedisClient::instance().setex(redis_key, 600, code);

            if (email_sender_) {
                email_sender_->send_delete_account_code(email, code);
            }
            APP_LOG_INFO("Delete account verification code for {}: {} (email sent: {})", email, code, email_sender_ != nullptr);

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateSuccess("注销验证码已发送到注册邮箱"));
            callback(resp);
        } catch (const std::exception& e) {
            APP_LOG_ERROR("Send delete code error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    void registerUser(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "请求格式无效"));
            callback(resp);
            return;
        }

        std::string email = (*json)["email"].asString();
        std::string username = (*json)["username"].asString();
        std::string password = (*json)["password"].asString();
        std::string code = (*json)["code"].asString();

        if (!Validator::is_valid_email(email)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "邮箱格式无效"));
            callback(resp);
            return;
        }

        if (!Validator::is_valid_username(username)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "用户名仅支持字母、数字、下划线，3-50位"));
            callback(resp);
            return;
        }

        if (!Validator::is_valid_password(password)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "密码需6-128位"));
            callback(resp);
            return;
        }

        std::string redis_key = std::string("verify_code:") + email + ":register";
        std::string stored_code = RedisClient::instance().get(redis_key);

        if (stored_code.empty() || stored_code != code) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "验证码无效或已过期"));
            callback(resp);
            return;
        }

        RedisClient::instance().del(redis_key);

        try {
            auto conn = MySQLClient::instance().acquire();

            auto check = conn->prepareStatement(
                "SELECT COUNT(*) FROM users WHERE email = ? OR username = ?");
            check->setString(1, email);
            check->setString(2, username);
            auto check_res = check->executeQuery();
            check_res->next();
            if (check_res->getInt(1) > 0) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(409, "邮箱或用户名已被注册"));
                MySQLClient::instance().release(std::move(conn));
                callback(resp);
                return;
            }

            uint64_t user_id = IDGenerator::instance().generate();
            std::string password_hash, password_salt;
            PasswordHasher::generate_hash_and_salt(password, password_hash, password_salt);

            auto stmt = conn->prepareStatement(
                "INSERT INTO users (id, username, password_hash, password_salt, email) "
                "VALUES (?, ?, ?, ?, ?)");
            stmt->setUInt64(1, user_id);
            stmt->setString(2, username);
            stmt->setString(3, password_hash);
            stmt->setString(4, password_salt);
            stmt->setString(5, email);
            stmt->executeUpdate();

            MySQLClient::instance().release(std::move(conn));

            auto resp = HttpResponse::newHttpResponse();
            auto result = Json::Value();
            result["code"] = 200;
            result["message"] = "注册成功";
            result["data"]["id"] = user_id;
            result["data"]["username"] = username;
            resp->setBody(result.toStyledString());
            callback(resp);

        } catch (const std::exception& e) {
            APP_LOG_ERROR("Register error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    void login(const HttpRequestPtr& req,
                std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "请求格式无效"));
            callback(resp);
            return;
        }

        std::string credential = (*json)["username"].asString();
        std::string password = (*json)["password"].asString();

        try {
            auto conn = MySQLClient::instance().acquire();

            bool is_email = credential.find('@') != std::string::npos;

            auto stmt = conn->prepareStatement(
                "SELECT id, username, password_hash, password_salt, email, "
                "role, status, total_chats, ban_expires_at, ban_reason "
                "FROM users WHERE " + std::string(is_email ? "email = ?" : "username = ?") +
                " AND status != 'deleted'");
            stmt->setString(1, credential);
            auto res = stmt->executeQuery();

            if (!res->next()) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(401, "用户名或密码错误"));
                callback(resp);
                return;
            }

            std::string password_hash = res->getString("password_hash");
            std::string password_salt = res->getString("password_salt");
            std::string status = res->getString("status");

            if (status == "pending_deletion") {
                MySQLClient::instance().release(std::move(conn));
                Json::Value result;
                result["code"] = 490;
                result["message"] = "账号正在注销冷静期内";
                result["data"]["pending_deletion"] = true;
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(result.toStyledString());
                callback(resp);
                return;
            }

            if (status == "banned") {
                Json::Value ban_data;
                std::string ban_expires_str;
                std::string ban_reason;
                int64_t remaining_seconds = 0;

                try { ban_expires_str = std::string(res->getString("ban_expires_at")); } catch (...) {}
                try { ban_reason = std::string(res->getString("ban_reason")); } catch (...) {}

                if (!ban_expires_str.empty()) {
                    auto now = std::chrono::system_clock::now();
                    auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
                        now.time_since_epoch()).count();

                    std::tm tm = {};
                    std::stringstream ss(ban_expires_str);
                    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    auto exp_ts = std::chrono::duration_cast<std::chrono::seconds>(
                        tp.time_since_epoch()).count();

                    remaining_seconds = exp_ts - now_ts;
                    if (remaining_seconds < 0) remaining_seconds = 0;
                }

                MySQLClient::instance().release(std::move(conn));

                Json::Value result;
                result["code"] = 403;
                result["message"] = "账号已被封禁";
                result["data"]["banned"] = true;
                result["data"]["ban_reason"] = ban_reason.empty() ? "违反平台规则" : ban_reason;
                result["data"]["remaining_seconds"] = static_cast<int64_t>(remaining_seconds);
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(result.toStyledString());
                callback(resp);
                return;
            }

            if (!PasswordHasher::verify_password(password, password_salt, password_hash)) {
                MySQLClient::instance().release(std::move(conn));
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(401, "用户名或密码错误"));
                callback(resp);
                return;
            }

            uint64_t user_id = res->getUInt64("id");
            std::string username = std::string(res->getString("username"));
            std::string role = res->getString("role");
            std::string email = res->getString("email");

            MySQLClient::instance().release(std::move(conn));

            std::string token = JWTUtils::generate_token(user_id, username, role);

            Json::Value session_data;
            session_data["user_id"] = user_id;
            session_data["username"] = username;
            session_data["role"] = role;
            RedisClient::instance().setex("session:" + token, 86400 * 7,
                                           session_data.toStyledString());

            RedisClient::instance().sadd("online_users", std::to_string(user_id));
            RedisClient::instance().sadd("user_sessions:" + std::to_string(user_id), token);

            auto resp = HttpResponse::newHttpResponse();
            Json::Value result;
            result["code"] = 200;
            result["message"] = "登录成功";
            result["data"]["token"] = token;
            result["data"]["user_id"] = user_id;
            result["data"]["username"] = username;
            result["data"]["email"] = email;
            result["data"]["role"] = role;
            resp->setBody(result.toStyledString());
            callback(resp);

        } catch (const std::exception& e) {
            APP_LOG_ERROR("Login error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    void resetPassword(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "请求格式无效"));
            callback(resp);
            return;
        }

        std::string email = (*json)["email"].asString();
        std::string code = (*json)["code"].asString();
        std::string new_password = (*json)["new_password"].asString();

        if (!Validator::is_valid_password(new_password)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "密码需6-128位"));
            callback(resp);
            return;
        }

        std::string redis_key = std::string("verify_code:") + email + ":reset_password";
        std::string stored_code = RedisClient::instance().get(redis_key);

        if (stored_code.empty() || stored_code != code) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(400, "验证码无效或已过期"));
            callback(resp);
            return;
        }

        RedisClient::instance().del(redis_key);

        try {
            auto conn = MySQLClient::instance().acquire();

            std::string password_hash, password_salt;
            PasswordHasher::generate_hash_and_salt(new_password, password_hash, password_salt);

            auto stmt = conn->prepareStatement(
                "UPDATE users SET password_hash = ?, password_salt = ? WHERE email = ?");
            stmt->setString(1, password_hash);
            stmt->setString(2, password_salt);
            stmt->setString(3, email);
            int affected = stmt->executeUpdate();

            MySQLClient::instance().release(std::move(conn));

            if (affected == 0) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(generateError(404, "邮箱不存在"));
                callback(resp);
                return;
            }

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateSuccess("密码重置成功"));
            callback(resp);

        } catch (const std::exception& e) {
            APP_LOG_ERROR("Reset password error: {}", e.what());
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(generateError(500, "服务器错误"));
            callback(resp);
        }
    }

    static void setEmailSender(std::shared_ptr<EmailSender> sender) {
        email_sender_ = sender;
    }

private:
    static std::shared_ptr<EmailSender> email_sender_;

    static std::string generateVerificationCode() {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 9);
        std::string code;
        for (int i = 0; i < 6; ++i) {
            code += std::to_string(dist(gen));
        }
        return code;
    }

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
