#pragma once

#include <drogon/HttpMiddleware.h>
#include <drogon/drogon.h>
#include "utils/JWTUtils.h"
#include "database/RedisClient.h"
#include "utils/Logger.h"

// AuthMiddleware
// JWT 认证中间件，拦截 HTTP 请求进行 Token 验证、来源检查和权限控制
class AuthMiddleware : public drogon::HttpMiddleware<AuthMiddleware> {
public:
    // invoke
    // 功能：中间件入口，执行 Token 验证、CSRF 来源检查和封禁校验
    // 参数：req - HTTP 请求对象；nextCb - 放行回调；mcb - 拦截回调
    // 说明：验证通过后在请求头注入用户信息（X-Token、X-User-Id、X-Username、X-User-Role）
    void invoke(const drogon::HttpRequestPtr& req,
                drogon::MiddlewareNextCallback&& nextCb,
                drogon::MiddlewareCallback&& mcb) override;
};
