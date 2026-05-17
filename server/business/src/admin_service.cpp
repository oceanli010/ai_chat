#include "business/admin_service.h"
#include "business/service_locator.h"
#include "infrastructure/logger.h"
#include "infrastructure/token_manager.h"
#include "data/user_repository.h"
#include "model/user.h"

nlohmann::json AdminService::getUsers(const std::string& token, int page, int limit, const std::string& search) {
    LOG_INFO("AdminService::getUsers");
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto user_repo = ServiceLocator::instance().userRepository();
    
    if (!token_mgr || !user_repo) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto payload = token_mgr->validateToken(token);
    if (!payload || payload->role != "admin") {
        return {{"status", "error"}, {"message", "Unauthorized"}};
    }
    
    int offset = (page - 1) * limit;
    auto users = user_repo->findAll(offset, limit);
    
    nlohmann::json result = {
        {"status", "ok"},
        {"users", nlohmann::json::array()}
    };
    
    for (const auto& user : users) {
        result["users"].push_back({
            {"id", user.id},
            {"email", user.email},
            {"nickname", user.nickname},
            {"role", user.role},
            {"banned", user.isBanned()},
            {"created_at", user.createdAt}
        });
    }
    
    return result;
}

nlohmann::json AdminService::banUser(const std::string& token, const std::string& userId, bool ban) {
    LOG_INFO("AdminService::banUser: userId={}, ban={}", userId, ban);
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto user_repo = ServiceLocator::instance().userRepository();
    
    if (!token_mgr || !user_repo) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto payload = token_mgr->validateToken(token);
    if (!payload || payload->role != "admin") {
        return {{"status", "error"}, {"message", "Unauthorized"}};
    }
    
    auto user = user_repo->findById(userId);
    if (!user) {
        return {{"status", "error"}, {"message", "User not found"}};
    }
    
    User updated = *user;
    updated.status = ban ? UserStatus::Banned : UserStatus::Active;
    updated.updatedAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (user_repo->update(updated)) {
        return {{"status", "ok"}, {"banned", ban}};
    }
    
    return {{"status", "error"}, {"message", "Failed to update user status"}};
}

nlohmann::json AdminService::getStats(const std::string& token) {
    LOG_INFO("AdminService::getStats");
    
    auto token_mgr = ServiceLocator::instance().tokenManager();
    auto user_repo = ServiceLocator::instance().userRepository();
    
    if (!token_mgr || !user_repo) {
        return {{"status", "error"}, {"message", "Service not available"}};
    }
    
    auto payload = token_mgr->validateToken(token);
    if (!payload || payload->role != "admin") {
        return {{"status", "error"}, {"message", "Unauthorized"}};
    }
    
    auto users = user_repo->findAll(0, 10000);
    int total_users = users.size();
    int banned_users = 0;
    
    for (const auto& user : users) {
        if (user.isBanned()) banned_users++;
    }
    
    return {
        {"status", "ok"},
        {"total_users", total_users},
        {"active_users", total_users - banned_users},
        {"banned_users", banned_users}
    };
}
