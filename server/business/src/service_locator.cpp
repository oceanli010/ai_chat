#include "business/service_locator.h"
#include "business/auth_service.h"
#include "business/user_service.h"
#include "business/chat_service.h"
#include "business/admin_service.h"

ServiceLocator& ServiceLocator::instance() {
    static ServiceLocator instance;
    return instance;
}

void ServiceLocator::setConnectionPool(std::shared_ptr<ConnectionPool> pool) { connection_pool_ = pool; }
void ServiceLocator::setRedisCache(std::shared_ptr<RedisCache> cache) { redis_cache_ = cache; }
void ServiceLocator::setTokenManager(std::shared_ptr<TokenManager> tm) { token_manager_ = tm; }
void ServiceLocator::setUserRepository(std::shared_ptr<UserRepository> repo) { user_repository_ = repo; }
void ServiceLocator::setChatRepository(std::shared_ptr<ChatRepository> repo) { chat_repository_ = repo; }

std::shared_ptr<ConnectionPool> ServiceLocator::connectionPool() const { return connection_pool_; }
std::shared_ptr<RedisCache> ServiceLocator::redisCache() const { return redis_cache_; }
std::shared_ptr<TokenManager> ServiceLocator::tokenManager() const { return token_manager_; }
std::shared_ptr<UserRepository> ServiceLocator::userRepository() const { return user_repository_; }
std::shared_ptr<ChatRepository> ServiceLocator::chatRepository() const { return chat_repository_; }

AuthService& ServiceLocator::getAuthService() {
    if (!auth_service_) {
        auth_service_ = std::make_unique<AuthService>();
    }
    return *auth_service_;
}

UserService& ServiceLocator::getUserService() {
    if (!user_service_) {
        user_service_ = std::make_unique<UserService>();
    }
    return *user_service_;
}

ChatService& ServiceLocator::getChatService() {
    if (!chat_service_) {
        chat_service_ = std::make_unique<ChatService>();
    }
    return *chat_service_;
}

AdminService& ServiceLocator::getAdminService() {
    if (!admin_service_) {
        admin_service_ = std::make_unique<AdminService>();
    }
    return *admin_service_;
}
