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
                drogon::MiddlewareCallback&& mcb) override;
};
