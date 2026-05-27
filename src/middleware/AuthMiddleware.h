#pragma once

#include <drogon/HttpMiddleware.h>
#include <drogon/drogon.h>
#include "utils/JWTUtils.h"
#include "database/RedisClient.h"
#include "utils/Logger.h"

class AuthMiddleware : public drogon::HttpMiddleware<AuthMiddleware> {
public:
    void invoke(const drogon::HttpRequestPtr& req,
                drogon::MiddlewareNextCallback&& nextCb,
                drogon::MiddlewareCallback&& mcb) override {
        auto path = req->path();

        static const std::vector<std::string> public_paths = {
            "/api/auth/send-code",
            "/api/auth/verify-code",

            "/api/auth/send-delete-code",
            "/api/auth/register",
            "/api/auth/login",
            "/api/auth/reset-password",
            "/api/user/cancel-delete",
            "/login.html",
            "/register.html",
            "/reset_password.html",
            "/admin.html",
            "/css/",
            "/js/",
            "/ws/",
            "/favicon.ico"
        };

        for (const auto& pp : public_paths) {
            if (path.find(pp) == 0) {
                nextCb(std::move(mcb));
                return;
            }
        }

        auto method = req->method();
        if (method == drogon::Post || method == drogon::Put || method == drogon::Delete) {
            auto origin = req->getHeader("Origin");
            if (!origin.empty()) {
                auto host = req->getHeader("Host");
                std::string origin_host;
                size_t pos = origin.find("://");
                if (pos != std::string::npos) {
                    origin_host = origin.substr(pos + 3);
                }
                size_t port_pos = origin_host.find(':');
                std::string origin_hostname = (port_pos != std::string::npos)
                    ? origin_host.substr(0, port_pos) : origin_host;

                size_t host_port_pos = host.find(':');
                std::string host_hostname = (host_port_pos != std::string::npos)
                    ? host.substr(0, host_port_pos) : host;

                if (origin_hostname != host_hostname) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k403Forbidden);
                    resp->setBody("{\"code\":403,\"message\":\"无效的请求来源\"}");
                    mcb(resp);
                    return;
                }
            }
        }

        auto auth_header = req->getHeader("Authorization");
        if (auth_header.empty()) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setBody("{\"code\":401,\"message\":\"未登录，请先登录\"}");
            mcb(resp);
            return;
        }

        std::string token = auth_header;
        if (token.substr(0, 7) == "Bearer ") {
            token = token.substr(7);
        }

        try {
            auto data = JWTUtils::verify_token(token);
            auto user_id = std::to_string(data.user_id);
            auto username = data.username;
            auto role = data.role;

            if (RedisClient::instance().sismember("banned_users", user_id)) {
                Json::Value ban_data;
                ban_data["banned"] = true;
                ban_data["ban_reason"] = "违反平台规则";

                std::string ban_info_str = RedisClient::instance().get("banned_info:" + user_id);
                if (!ban_info_str.empty()) {
                    Json::Value ban_info;
                    Json::Reader reader;
                    if (reader.parse(ban_info_str, ban_info)) {
                        if (ban_info.isMember("ban_reason")) {
                            ban_data["ban_reason"] = ban_info["ban_reason"].asString();
                        }
                        if (ban_info.isMember("banned_at") && ban_info.isMember("duration_hours")) {
                            auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            int64_t banned_at = std::stoll(ban_info["banned_at"].asString());
                            int64_t duration_seconds = static_cast<int64_t>(ban_info["duration_hours"].asInt()) * 3600;
                            int64_t remaining = banned_at + duration_seconds - now_ts;
                            if (remaining < 0) remaining = 0;
                            ban_data["remaining_seconds"] = remaining;
                        }
                    }
                }

                auto session_key = "session:" + token;
                RedisClient::instance().del(session_key);
                RedisClient::instance().srem("online_users", user_id);

                Json::Value result;
                result["code"] = 403;
                result["message"] = "账号已被封禁";
                result["data"] = ban_data;

                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setBody(result.toStyledString());
                mcb(resp);
                return;
            }

            auto session_key = "session:" + token;
            if (!RedisClient::instance().exists(session_key)) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k401Unauthorized);
                resp->setBody("{\"code\":401,\"message\":\"会话已过期，请重新登录\"}");
                mcb(resp);
                return;
            }

            req->addHeader("X-Token", token);
            req->addHeader("X-User-Id", user_id);
            req->addHeader("X-Username", username);
            req->addHeader("X-User-Role", role);

            if (path.find("/api/admin/") == 0 && role != "admin") {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setBody("{\"code\":403,\"message\":\"仅管理员可访问\"}");
                mcb(resp);
                return;
            }

            nextCb(std::move(mcb));
        } catch (const std::exception& e) {
            APP_LOG_WARN("Auth failed: {}", e.what());
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k401Unauthorized);
            resp->setBody("{\"code\":401,\"message\":\"Token无效，请重新登录\"}");
            mcb(resp);
        }
    }
};
