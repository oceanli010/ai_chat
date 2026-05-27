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

    ADD_METHOD_TO(AuthController::sendDeleteCode, "/api/auth/send-delete-code", Post);
    ADD_METHOD_TO(AuthController::registerUser, "/api/auth/register", Post);
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", Post);
    ADD_METHOD_TO(AuthController::resetPassword, "/api/auth/reset-password", Post);
    METHOD_LIST_END

    void sendCode(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback);

    void sendDeleteCode(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback);

    void registerUser(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback);

    void login(const HttpRequestPtr& req,
                std::function<void(const HttpResponsePtr&)>&& callback);

    void resetPassword(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback);

    static void setEmailSender(std::shared_ptr<EmailSender> sender);

private:
    static std::shared_ptr<EmailSender> email_sender_;

    static std::string generateVerificationCode();
    static std::string generateError(int code, const std::string& message);
    static std::string generateSuccess(const std::string& message);
};
