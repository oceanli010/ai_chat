#include <drogon/drogon.h>
#include <fstream>
#include <iostream>
#include "utils/Logger.h"
#include "utils/JWTUtils.h"
#include "database/MySQLClient.h"
#include "database/RedisClient.h"
#include "controllers/AuthController.h"
#include "controllers/UserController.h"
#include "controllers/ChatController.h"
#include "controllers/AdminController.h"
#include "controllers/NotificationController.h"
#include "middleware/AuthMiddleware.h"

std::shared_ptr<EmailSender> AdminController::email_sender_ = nullptr;

using namespace drogon;

Json::Value loadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_path << std::endl;
        exit(1);
    }

    Json::Value config;
    Json::Reader reader;
    if (!reader.parse(file, config)) {
        std::cerr << "Failed to parse config file" << std::endl;
        exit(1);
    }

    return config;
}

void initAdminAccount(const Json::Value& config) {
    std::string admin_username = config["admin"]["username"].asString();
    std::string admin_password = config["admin"]["password"].asString();

    auto conn = MySQLClient::instance().acquire();

    auto check = conn->prepareStatement(
        "SELECT COUNT(*) FROM users WHERE username = ?");
    check->setString(1, admin_username);
    auto res = check->executeQuery();
    res->next();

    if (res->getInt(1) == 0) {
        uint64_t admin_id = IDGenerator::instance().generate();
        std::string password_hash, password_salt;
        PasswordHasher::generate_hash_and_salt(admin_password, password_hash, password_salt);

        auto stmt = conn->prepareStatement(
            "INSERT INTO users (id, username, password_hash, password_salt, "
            "email, role) VALUES (?, ?, ?, ?, ?, 'admin')");
        stmt->setUInt64(1, admin_id);
        stmt->setString(2, admin_username);
        stmt->setString(3, password_hash);
        stmt->setString(4, password_salt);
        stmt->setString(5, admin_username + "@admin.local");
        stmt->executeUpdate();

        APP_LOG_INFO("Admin account created: {} (ID: {})", admin_username, admin_id);
    } else {
        APP_LOG_INFO("Admin account already exists");
    }

    MySQLClient::instance().release(std::move(conn));
}

void scheduleCleanupTask() {
    app().getLoop()->runEvery(60.0, []() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm* tm_now = std::localtime(&time_t_now);

        try {
            auto conn = MySQLClient::instance().acquire();

            auto stmt = conn->prepareStatement(
                "DELETE FROM chat_messages WHERE created_at < "
                "DATE_SUB(NOW(), INTERVAL 30 DAY)");
            int deleted = stmt->executeUpdate();
            if (deleted > 0) APP_LOG_INFO("Cleaned {} old chat messages", deleted);

            auto ver_stmt = conn->prepareStatement(
                "DELETE FROM email_verifications WHERE expires_at < NOW()");
            int ver_deleted = ver_stmt->executeUpdate();
            if (ver_deleted > 0) APP_LOG_INFO("Cleaned {} expired verification codes", ver_deleted);

            auto pend_stmt = conn->prepareStatement(
                "SELECT id FROM users WHERE status = 'pending_deletion' AND deleted_at <= NOW()");
            auto pend_res = pend_stmt->executeQuery();
            while (pend_res->next()) {
                uint64_t uid = pend_res->getUInt64("id");
                auto del = conn->prepareStatement("DELETE FROM users WHERE id = ?");
                del->setUInt64(1, uid);
                del->executeUpdate();
                APP_LOG_INFO("Auto-deleted account {} (cooling period expired)", uid);
            }

            MySQLClient::instance().release(std::move(conn));
        } catch (const std::exception& e) {
            APP_LOG_ERROR("Cleanup task error: {}", e.what());
        }
    });
}

int main(int argc, char* argv[]) {
    std::string config_path = "config/config.json";
    if (argc > 1) {
        config_path = argv[1];
    }

    auto config = loadConfig(config_path);

    auto& log_cfg = config["log"];
    Logger::init(log_cfg["file"].asString(),
                 log_cfg["level"].asString(),
                 log_cfg["max_size"].asUInt64(),
                 log_cfg["max_files"].asUInt());

    APP_LOG_INFO("Starting AI Chat server...");

    auto& mysql_cfg = config["database"]["mysql"];
    MySQLClient::instance().init(
        mysql_cfg["host"].asString(),
        mysql_cfg["port"].asInt(),
        mysql_cfg["user"].asString(),
        mysql_cfg["password"].asString(),
        mysql_cfg["database"].asString(),
        mysql_cfg["pool_size"].asInt());

    auto& redis_cfg = config["database"]["redis"];
    RedisClient::instance().init(
        redis_cfg["host"].asString(),
        redis_cfg["port"].asInt(),
        redis_cfg["password"].asString(),
        redis_cfg["db"].asInt());

    initAdminAccount(config);

    JWTUtils::init("ai_chat_jwt_secret_key_2024");

    auto email_cfg = config["email"];
    if (!email_cfg["username"].asString().empty()) {
        auto email_sender = std::make_shared<EmailSender>(
            email_cfg["smtp_host"].asString(),
            email_cfg["smtp_port"].asInt(),
            email_cfg["username"].asString(),
            email_cfg["password"].asString(),
            email_cfg["from_address"].asString());
        AuthController::setEmailSender(email_sender);
        AdminController::setEmailSender(email_sender);
        APP_LOG_INFO("Email sender configured");
    } else {
        APP_LOG_WARN("Email not configured, verification codes will be logged only");
    }

    auto& ai_cfg = config["ai"];
    ChatController::setAIAPIUrl(ai_cfg["api_url"].asString());
    ChatController::setAIAPIKey(ai_cfg["api_key"].asString());
    ChatController::setAIModel(ai_cfg["model"].asString());

    scheduleCleanupTask();

    auto& server_cfg = config["server"];
    app().addListener("0.0.0.0", server_cfg["port"].asInt());

    app().setThreadNum(server_cfg["thread_num"].asInt());

    if (!server_cfg["document_root"].asString().empty()) {
        app().setDocumentRoot(server_cfg["document_root"].asString());
    }

    app().setUploadPath("./uploads");
    app().enableSession(false);

    app().setLogPath("logs/");
    app().setLogLevel(trantor::Logger::kInfo);

    APP_LOG_INFO("Server starting on port {}", server_cfg["port"].asInt());
    APP_LOG_INFO("Document root: {}", server_cfg["document_root"].asString());

    app().run();

    return 0;
}
