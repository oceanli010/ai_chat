#include "business/user_service.h"
#include "business/service_locator.h"
#include "infrastructure/logger.h"
#include "infrastructure/crypto_util.h"
#include "infrastructure/token_manager.h"
#include "infrastructure/email_sender.h"
#include "data/user_repository.h"
#include "data/redis_cache.h"
#include "model/user.h"

nlohmann::json UserService::sendVerificationCode(const std::string& email) {
    LOG_INFO("UserService::sendVerificationCode: {}", email);
    
    auto redis = ServiceLocator::instance().redisCache();
    if (!redis) {
        return {{"status", "error"}, {"message", "Redis not available"}};
    }
    
    std::string code = std::to_string(rand() % 900000 + 100000);
    redis->setEx("verify_code:" + email, 300, code);
    
    LOG_INFO("Verification code for {}: {}", email, code);
    
    EmailSender sender;
    std::string subject = "AI Chat - Verification Code";
    std::string body = "Your verification code is: " + code + "\n\nThe code will expire in 5 minutes.\n\nIf you did not request this, please ignore this email.";
    
    if (!sender.send(email, subject, body)) {
        LOG_ERROR("Failed to send verification code email to: {}", email);
        return {{"status", "error"}, {"message", "Failed to send verification code email"}, {"code", code}};
    }
    
    LOG_INFO("Verification code sent to: {}", email);
    return {{"status", "ok"}};
}

nlohmann::json UserService::registerUser(const std::string& email, const std::string& code, const std::string& nickname, const std::string& password) {
    LOG_INFO("UserService::registerUser: {}", email);
    
    auto redis = ServiceLocator::instance().redisCache();
    if (!redis) {
        return {{"status", "error"}, {"message", "Redis not available"}};
    }
    
    std::string stored_code = redis->get("verify_code:" + email);
    if (stored_code.empty() || stored_code != code) {
        return {{"status", "error"}, {"message", "Invalid verification code"}};
    }
    
    auto user_repo = ServiceLocator::instance().userRepository();
    if (!user_repo) {
        return {{"status", "error"}, {"message", "User repository not available"}};
    }
    
    if (user_repo->findByEmail(email)) {
        return {{"status", "error"}, {"message", "Email already registered"}};
    }
    
    std::string hash = CryptoUtil::hashPassword(password);
    
    User user;
    user.id = CryptoUtil::generateUUID();
    user.email = email;
    user.nickname = nickname;
    user.passwordHash = hash;
    user.role = "user";
    user.createdAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (user_repo->save(user)) {
        redis->del("verify_code:" + email);
        
        auto token_mgr = ServiceLocator::instance().tokenManager();
        std::string access_token = token_mgr ? token_mgr->generateAccessToken(user.id, user.role) : "";
        
        return {
            {"status", "ok"},
            {"token", access_token},
            {"user", {
                {"id", user.id},
                {"email", user.email},
                {"nickname", user.nickname},
                {"role", user.role}
            }}
        };
    }
    
    return {{"status", "error"}, {"message", "Failed to register user"}};
}

nlohmann::json UserService::getProfile(const std::string& token) {
    LOG_INFO("UserService::getProfile");
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto user_repo = ServiceLocator::instance().userRepository();
    
    if (!token_mgr || !user_repo) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto payload = token_mgr->validateToken(token);
    if (!payload) {
        return {{"status", "error"}, {"message", "Invalid token"}};
    }
    
    auto user = user_repo->findById(payload->user_id);
    
    if (user) {
        return {
            {"status", "ok"},
            {"user", {
                {"id", user->id},
                {"email", user->email},
                {"nickname", user->nickname},
                {"role", user->role},
                {"created_at", user->createdAt}
            }}
        };
    }
    
    return {{"status", "error"}, {"message", "User not found"}};
}

nlohmann::json UserService::updateNickname(const std::string& token, const std::string& nickname) {
    LOG_INFO("UserService::updateNickname");
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto user_repo = ServiceLocator::instance().userRepository();
    
    if (!token_mgr || !user_repo) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto payload = token_mgr->validateToken(token);
    if (!payload) {
        return {{"status", "error"}, {"message", "Invalid token"}};
    }
    
    if (user_repo->updateNickname(payload->user_id, nickname)) {
        return {{"status", "ok"}};
    }
    
    return {{"status", "error"}, {"message", "Failed to update nickname"}};
}

nlohmann::json UserService::deleteAccount(const std::string& token) {
    LOG_INFO("UserService::deleteAccount");
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto user_repo = ServiceLocator::instance().userRepository();
    
    if (!token_mgr || !user_repo) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto payload = token_mgr->validateToken(token);
    if (!payload) {
        return {{"status", "error"}, {"message", "Invalid token"}};
    }
    
    if (user_repo->remove(payload->user_id)) {
        return {{"status", "ok"}};
    }
    
    return {{"status", "error"}, {"message", "Failed to delete account"}};
}
