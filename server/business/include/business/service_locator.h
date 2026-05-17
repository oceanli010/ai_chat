#pragma once

#include <memory>

class ConnectionPool;
class RedisCache;
class TokenManager;
class UserRepository;
class ChatRepository;
class AuthService;
class UserService;
class ChatService;
class AdminService;

class ServiceLocator {
public:
    static ServiceLocator& instance();

    void setConnectionPool(std::shared_ptr<ConnectionPool> pool);
    void setRedisCache(std::shared_ptr<RedisCache> cache);
    void setTokenManager(std::shared_ptr<TokenManager> tm);
    void setUserRepository(std::shared_ptr<UserRepository> repo);
    void setChatRepository(std::shared_ptr<ChatRepository> repo);

    std::shared_ptr<ConnectionPool> connectionPool() const;
    std::shared_ptr<RedisCache> redisCache() const;
    std::shared_ptr<TokenManager> tokenManager() const;
    std::shared_ptr<UserRepository> userRepository() const;
    std::shared_ptr<ChatRepository> chatRepository() const;

    AuthService& getAuthService();
    UserService& getUserService();
    ChatService& getChatService();
    AdminService& getAdminService();

private:
    ServiceLocator() = default;

    std::shared_ptr<ConnectionPool> connection_pool_;
    std::shared_ptr<RedisCache> redis_cache_;
    std::shared_ptr<TokenManager> token_manager_;
    std::shared_ptr<UserRepository> user_repository_;
    std::shared_ptr<ChatRepository> chat_repository_;

    std::unique_ptr<AuthService> auth_service_;
    std::unique_ptr<UserService> user_service_;
    std::unique_ptr<ChatService> chat_service_;
    std::unique_ptr<AdminService> admin_service_;
};
