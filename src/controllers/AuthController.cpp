#include "AuthController.h"

std::shared_ptr<EmailSender> AuthController::email_sender_ = nullptr;

// setEmailSender
// 功能：设置全局邮件发送器实例
// 参数：sender - 邮件发送器的共享指针
void AuthController::setEmailSender(std::shared_ptr<EmailSender> sender) {
    email_sender_ = sender;
}

// sendCode
// 功能：发送邮箱验证码（注册或重置密码）
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：包含 IP 级别频率限制（5 分钟内最多 10 次），验证码有效期 5 分钟
void AuthController::sendCode(const HttpRequestPtr& req,
                               std::function<void(const HttpResponsePtr&)>&& callback) {
    // IP 级别频率限制，5 分钟内最多 10 次
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
    // 将验证码存入 Redis，有效期 5 分钟
    RedisClient::instance().setex(redis_key, 300, code);

    bool email_sent = false;
    if (email_sender_) {
        email_sent = email_sender_->send_verification_code(email, code);
    }

    APP_LOG_INFO("Verification code for {} (email sent: {})", email, email_sent);

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(generateSuccess("验证码已发送"));
    callback(resp);
}

// sendDeleteCode
// 功能：发送账号注销验证码到用户注册邮箱
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：需要用户已登录，验证码有效期 10 分钟，包含 IP 级别频率限制
void AuthController::sendDeleteCode(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string rate_key = "rate_limit:" + std::string(__func__) + ":" + req->getPeerAddr().toIp();
    auto attempts = RedisClient::instance().incr(rate_key);
    if (attempts == 1) RedisClient::instance().expire(rate_key, 300);
    if (attempts > 10) { auto resp = HttpResponse::newHttpResponse(); resp->setBody(generateError(429, "操作过于频繁，请稍后再试")); callback(resp); return; }
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
        // 注销验证码有效期 10 分钟
        RedisClient::instance().setex(redis_key, 600, code);

        if (email_sender_) {
            email_sender_->send_delete_account_code(email, code);
        }
        APP_LOG_INFO("Delete account verification code for {} (email sent: {})", email, email_sender_ != nullptr);

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

// registerUser
// 功能：用户注册
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：需要邮箱验证码，校验用户名和密码格式，使用分布式 ID 生成器创建用户 ID，密码加盐哈希存储
void AuthController::registerUser(const HttpRequestPtr& req,
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

    // 校验验证码
    std::string redis_key = std::string("verify_code:") + email + ":register";
    std::string stored_code = RedisClient::instance().get(redis_key);

    if (stored_code.empty() || stored_code != code) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(generateError(400, "验证码无效或已过期"));
        callback(resp);
        return;
    }

    // 验证码使用后立即删除，防止重复使用
    RedisClient::instance().del(redis_key);

    try {
        auto conn = MySQLClient::instance().acquire();

        // 检查邮箱或用户名是否已被注册
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
        // 生成密码哈希和盐值
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

// login
// 功能：用户登录
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：支持邮箱或用户名登录，校验密码后生成 JWT token，记录在线状态和会话信息
void AuthController::login(const HttpRequestPtr& req,
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

    std::string credential = (*json)["username"].asString();
    std::string password = (*json)["password"].asString();

    try {
        auto conn = MySQLClient::instance().acquire();

        // 自动检测使用邮箱还是用户名登录
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

        // 处理账号处于注销冷静期的登录拦截
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

        // 处理账号被封禁的情况，返回封禁剩余时间
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

        // 验证密码
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

        // 生成 JWT token，有效期 7 天
        std::string token = JWTUtils::generate_token(user_id, username, role);

        // 保存会话信息到 Redis
        Json::Value session_data;
        session_data["user_id"] = user_id;
        session_data["username"] = username;
        session_data["role"] = role;
        RedisClient::instance().setex("session:" + token, 86400 * 7,
                                       session_data.toStyledString());

        // 记录在线用户和用户会话列表
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

// resetPassword
// 功能：通过邮箱验证码重置密码
// 参数：req - HTTP 请求对象；callback - 异步响应回调
// 说明：需要邮箱验证码，新密码需满足密码格式要求
void AuthController::resetPassword(const HttpRequestPtr& req,
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

    // 校验验证码
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

// generateVerificationCode
// 功能：生成 6 位数字验证码
// 返回值：string - 6 位数字字符串
std::string AuthController::generateVerificationCode() {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 9);
    std::string code;
    for (int i = 0; i < 6; ++i) {
        code += std::to_string(dist(gen));
    }
    return code;
}

// generateError
// 功能：生成错误响应 JSON 字符串
// 参数：code - 错误状态码；message - 错误信息
// 返回值：string - JSON 格式的错误响应字符串
std::string AuthController::generateError(int code, const std::string& message) {
    Json::Value result;
    result["code"] = code;
    result["message"] = message;
    return result.toStyledString();
}

// generateSuccess
// 功能：生成成功响应 JSON 字符串
// 参数：message - 成功信息
// 返回值：string - JSON 格式的成功响应字符串
std::string AuthController::generateSuccess(const std::string& message) {
    Json::Value result;
    result["code"] = 200;
    result["message"] = message;
    return result.toStyledString();
}
