#include "business/auth_service.h"
#include "business/service_locator.h"
#include "infrastructure/logger.h"
#include "infrastructure/crypto_util.h"
#include "infrastructure/token_manager.h"
#include "data/user_repository.h"
#include "data/redis_cache.h"
#include "model/user.h"

nlohmann::json AuthService::login(const std::string& email, const std::string& password) {
    LOG_INFO("AuthService::login: {}", email);
    
    auto user_repo = ServiceLocator::instance().userRepository();
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto redis = ServiceLocator::instance().redisCache();
    
    if (!user_repo || !token_mgr) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto user = user_repo->findByEmail(email);
    if (!user) {
        return {{"status", "error"}, {"message", "Invalid email or password"}};
    }
    
    if (user->isBanned()) {
        return {{"status", "error"}, {"message", "Account has been banned"}};
    }
    
    if (!CryptoUtil::verifyPassword(password, user->passwordHash)) {
        return {{"status", "error"}, {"message", "Invalid email or password"}};
    }
    
    std::string token = token_mgr->generateAccessToken(user->id, user->role);
    
    if (redis) {
        redis->setEx("token:" + user->id, 86400, token);
    }
    
    return {
        {"status", "ok"},
        {"token", token},
        {"user", {
            {"id", user->id},
            {"email", user->email},
            {"nickname", user->nickname},
            {"role", user->role}
        }}
    };
}

nlohmann::json AuthService::refresh(const std::string& refreshToken) {
    LOG_INFO("AuthService::refresh");
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    if (!token_mgr) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto payload = token_mgr->validateToken(refreshToken);
    if (!payload) {
        return {{"status", "error"}, {"message", "Invalid token"}};
    }
    
    std::string new_token = token_mgr->generateAccessToken(payload->user_id, payload->role);
    
    return {{"status", "ok"}, {"token", new_token}};
}

bool AuthService::logout(const std::string& accessToken) {
    LOG_INFO("AuthService::logout");
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto redis = ServiceLocator::instance().redisCache();
    
    if (!token_mgr) {
        return false;
    }
    
    auto payload = token_mgr->validateToken(accessToken);
    if (payload && redis) {
        redis->del("token:" + payload->user_id);
    }
    
    return true;
}
