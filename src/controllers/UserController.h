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

// UserController
// 功能：处理用户个人中心相关的业务逻辑，包括资料管理、密码修改和账号注销
class UserController : public HttpController<UserController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::getProfile, "/api/user/profile", Get);
    ADD_METHOD_TO(UserController::updateProfile, "/api/user/profile", Put);
    ADD_METHOD_TO(UserController::changePassword, "/api/user/password", Put);
    ADD_METHOD_TO(UserController::deleteAccount, "/api/user/account", Delete);
    ADD_METHOD_TO(UserController::cancelDelete, "/api/user/cancel-delete", Post);
    METHOD_LIST_END

    uint64_t getUserIdFromToken(const HttpRequestPtr& req);

    void getProfile(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);

    void updateProfile(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback);

    void changePassword(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback);

    void deleteAccount(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback);

    void cancelDelete(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback);

private:
    static std::string generateError(int code, const std::string& message);
    static std::string generateSuccess(const std::string& message);
};
