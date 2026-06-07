#include <drogon/drogon.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <random>
#include <sstream>
#include <iomanip>
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

using namespace drogon;

// get_env_or
// 功能：读取环境变量，若不存在则返回默认值（容器化适配）
// 参数：env_name - 环境变量名；default_val - 默认值
// 返回值：string - 环境变量值或默认值
static std::string get_env_or(const char* env_name, const std::string& default_val) {
    const char* val = std::getenv(env_name);
    return val ? std::string(val) : default_val;
}

// loadConfig
// 功能：从配置文件路径读取并解析 JSON 格式的配置
// 参数：config_path - 配置文件路径
// 返回值：Json::Value - 解析后的 JSON 配置对象
// 说明：文件打开或解析失败时直接退出程序
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

// initAdminAccount
// 功能：检查并创建初始管理员账号（若不存在）
// 参数：config - 服务器配置对象，从中读取管理员用户名和密码
// 说明：使用预置密码生成哈希和加盐后写入数据库，角色固定为 admin
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
}

// scheduleCleanupTask
// 功能：注册定时清理任务，每 60 秒执行一次
// 说明：清理任务包含——①删除 30 天前的聊天记录；②删除过期的邮箱验证码；③删除冷却期已到的待注销账号
void scheduleCleanupTask() {
    app().getLoop()->runEvery(60.0, []() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm* tm_now = std::localtime(&time_t_now);

        try {
            auto conn = MySQLClient::instance().acquire();

            // 清理 30 天前的聊天消息
            auto stmt = conn->prepareStatement(
                "DELETE FROM chat_messages WHERE created_at < "
                "DATE_SUB(NOW(), INTERVAL 30 DAY)");
            int deleted = stmt->executeUpdate();
            if (deleted > 0) APP_LOG_INFO("Cleaned {} old chat messages", deleted);

            // 清理已过期的邮箱验证码
            auto ver_stmt = conn->prepareStatement(
                "DELETE FROM email_verifications WHERE expires_at < NOW()");
            int ver_deleted = ver_stmt->executeUpdate();
            if (ver_deleted > 0) APP_LOG_INFO("Cleaned {} expired verification codes", ver_deleted);

            // 查询并删除冷却期已过的待注销账号
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

// main
// 功能：AI Chat 服务器主入口，完成配置加载、数据库初始化、中间件注册和 HTTP 服务启动
// 参数：argc - 命令行参数个数；argv - 命令行参数数组
// 返回值：int - 程序退出码
// 说明：启动流程依次为——①加载配置文件；②初始化日志系统；③初始化 MySQL 和 Redis 连接池；
//       ④创建管理员账号；⑤初始化 JWT 密钥；⑥配置邮件发送器和 AI 接口参数；
//       ⑦注册定时清理任务；⑧配置 HTTPS/HTTP 监听、线程数和安全策略；⑨启动 Drogon 事件循环
int main(int argc, char* argv[]) {
    std::string config_path = "config/config.json";
    if (argc > 1) {
        config_path = argv[1];
    }

    auto config = loadConfig(config_path);

    // 初始化日志系统
    auto& log_cfg = config["log"];
    Logger::init(log_cfg["file"].asString(),
                 log_cfg["level"].asString(),
                 log_cfg["max_size"].asUInt64(),
                 log_cfg["max_files"].asUInt());

    APP_LOG_INFO("Starting AI Chat server...");

    // 初始化 MySQL 连接池
    auto& mysql_cfg = config["database"]["mysql"];
    // 环境变量覆盖数据库连接配置（容器化适配）
    mysql_cfg["host"] = get_env_or("MYSQL_HOST", mysql_cfg["host"].asString());
    mysql_cfg["port"] = std::stoi(get_env_or("MYSQL_PORT", std::to_string(mysql_cfg["port"].asInt())));
    mysql_cfg["user"] = get_env_or("MYSQL_USER", mysql_cfg["user"].asString());
    mysql_cfg["password"] = get_env_or("MYSQL_PASSWORD", mysql_cfg["password"].asString());
    mysql_cfg["database"] = get_env_or("MYSQL_DATABASE", mysql_cfg["database"].asString());
    MySQLClient::instance().init(
        mysql_cfg["host"].asString(),
        mysql_cfg["port"].asInt(),
        mysql_cfg["user"].asString(),
        mysql_cfg["password"].asString(),
        mysql_cfg["database"].asString(),
        mysql_cfg["pool_size"].asInt());

    // 初始化 Redis 连接
    auto& redis_cfg = config["database"]["redis"];
    // 环境变量覆盖 Redis 连接配置（容器化适配）
    redis_cfg["host"] = get_env_or("REDIS_HOST", redis_cfg["host"].asString());
    redis_cfg["port"] = std::stoi(get_env_or("REDIS_PORT", std::to_string(redis_cfg["port"].asInt())));
    redis_cfg["password"] = get_env_or("REDIS_PASSWORD", redis_cfg["password"].asString());
    RedisClient::instance().init(
        redis_cfg["host"].asString(),
        redis_cfg["port"].asInt(),
        redis_cfg["password"].asString(),
        redis_cfg["db"].asInt());

    initAdminAccount(config);

    // 初始化 JWT 密钥（配置中为空时自动生成随机密钥）
    auto jwt_secret = config["server"]["jwt_secret"].asString();
    if (jwt_secret.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::stringstream ss;
        for (int i = 0; i < 64; ++i) {
            ss << std::hex << dis(gen);
        }
        jwt_secret = ss.str();
        APP_LOG_WARN("jwt_secret is empty in config, using auto-generated random secret");
    }
    JWTUtils::init(jwt_secret);

    // 配置邮件发送器
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

    // 配置 AI 接口参数
    auto& ai_cfg = config["ai"];
    ChatController::setAIAPIUrl(ai_cfg["api_url"].asString());
    ChatController::setAIAPIKey(ai_cfg["api_key"].asString());
    ChatController::setAIModel(ai_cfg["model"].asString());

    // 注册定时清理任务
    scheduleCleanupTask();

    // 配置 HTTP/HTTPS 监听和线程数
    auto& server_cfg = config["server"];
    bool use_https = server_cfg.isMember("enable_https") && server_cfg["enable_https"].asBool();
    if (use_https) {
        auto cert = server_cfg["https_cert"].asString();
        auto key = server_cfg["https_key"].asString();
        app().addListener("0.0.0.0", server_cfg["port"].asInt(), true, cert, key);
    } else {
        app().addListener("0.0.0.0", server_cfg["port"].asInt());
    }

    // 配置线程数：配置中设为 0 或负数时自动使用 CPU 核数
    int thread_num = server_cfg["thread_num"].asInt();
    if (thread_num <= 0) {
        thread_num = static_cast<int>(std::thread::hardware_concurrency());
    }
    app().setThreadNum(thread_num);

    if (!server_cfg["document_root"].asString().empty()) {
        app().setDocumentRoot(server_cfg["document_root"].asString());
    }

    app().setUploadPath("./uploads");
    app().enableSession(false);

    // 静态文件缓存 1 小时，减少重复文件读取
    app().setStaticFilesCacheTime(3600);

    app().setLogPath("logs/");
    app().setLogLevel(trantor::Logger::kInfo);

    // 注册全局响应头：Content-Security-Policy 安全策略
    app().registerPostHandlingAdvice([](const HttpRequestPtr& req, const HttpResponsePtr& resp) {
        (void)req;
        resp->addHeader("Content-Security-Policy",
            "default-src 'self'; "
            "script-src 'self' 'unsafe-inline'; "
            "style-src 'self' 'unsafe-inline'; "
            "img-src 'self' data:; "
            "font-src 'self'; "
            "connect-src 'self'; "
            "frame-ancestors 'none'; "
            "form-action 'self'"
        );
    });

    // 健康检查端点（容器化适配：供 Docker HEALTHCHECK 和 K8s 探针使用）
    app().registerHandler(
        "/health",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            (void)req;
            Json::Value resp;
            resp["status"] = "ok";
            resp["timestamp"] = (Json::Int64)std::time(nullptr);
            auto http_resp = HttpResponse::newHttpJsonResponse(resp);
            callback(http_resp);
        },
        {Get}
    );

    APP_LOG_INFO("Server starting on {}://0.0.0.0:{}",
                 use_https ? "https" : "http",
                 server_cfg["port"].asInt());
    APP_LOG_INFO("Document root: {}", server_cfg["document_root"].asString());

    app().run();

    return 0;
}
