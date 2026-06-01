#pragma once

#include <drogon/drogon.h>
#include <drogon/HttpController.h>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include "database/MySQLClient.h"
#include "database/RedisClient.h"
#include "utils/EmailSender.h"
#include "utils/Logger.h"
#include "controllers/NotificationController.h"

using namespace drogon;

class AdminController : public HttpController<AdminController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminController::getStats, "/api/admin/stats", Get);
    ADD_METHOD_TO(AdminController::getUsers, "/api/admin/users", Get);
    ADD_METHOD_TO(AdminController::searchUsers, "/api/admin/search", Post);
    ADD_METHOD_TO(AdminController::banUser, "/api/admin/ban", Post);
    ADD_METHOD_TO(AdminController::getLogs, "/api/admin/logs", Get);
    ADD_METHOD_TO(AdminController::deleteUser, "/api/admin/delete-user", Post);
    METHOD_LIST_END

    void getStats(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback);

    void getUsers(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback);

    void searchUsers(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);

    void banUser(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback);

    void getLogs(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback);

    void deleteUser(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);

    static void setEmailSender(std::shared_ptr<EmailSender> sender);

private:
    static std::shared_ptr<EmailSender> email_sender_;

    static std::string generateError(int code, const std::string& message);
    static std::string generateSuccess(const std::string& message);
};
