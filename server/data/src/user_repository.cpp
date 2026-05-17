#include "data/user_repository.h"
#include "infrastructure/logger.h"
#include "data/mysql_database.h"

UserRepository::UserRepository(std::shared_ptr<ConnectionPool> pool)
    : pool_(std::move(pool)) {}

std::optional<User> UserRepository::findById(const std::string& id) {
    LOG_INFO("UserRepository::findById: {}", id);
    
    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return std::nullopt;
    }
    
    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return std::nullopt;
    }
    
    try {
        std::string query = "SELECT id, email, password_hash, nickname, role, status, created_at, updated_at, last_login_at FROM users WHERE id = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, id);
        
        auto result = stmt->executeQuery();
        if (result->next()) {
            User user;
            user.id = result->getString("id");
            user.email = result->getString("email");
            user.passwordHash = result->getString("password_hash");
            user.nickname = result->getString("nickname");
            user.role = result->getString("role");
            user.status = result->getString("status") == "active" ? UserStatus::Active : UserStatus::Banned;
            user.createdAt = result->getInt64("created_at");
            user.updatedAt = result->getInt64("updated_at");
            user.lastLoginAt = result->getInt64("last_login_at");
            return user;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("UserRepository::findById error: {}", e.what());
    }
    
    return std::nullopt;
}

std::optional<User> UserRepository::findByEmail(const std::string& email) {
    LOG_INFO("UserRepository::findByEmail: {}", email);
    
    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return std::nullopt;
    }
    
    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return std::nullopt;
    }
    
    try {
        std::string query = "SELECT id, email, password_hash, nickname, role, status, created_at, updated_at, last_login_at FROM users WHERE email = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, email);
        
        auto result = stmt->executeQuery();
        if (result->next()) {
            User user;
            user.id = result->getString("id");
            user.email = result->getString("email");
            user.passwordHash = result->getString("password_hash");
            user.nickname = result->getString("nickname");
            user.role = result->getString("role");
            user.status = result->getString("status") == "active" ? UserStatus::Active : UserStatus::Banned;
            user.createdAt = result->getInt64("created_at");
            user.updatedAt = result->getInt64("updated_at");
            user.lastLoginAt = result->getInt64("last_login_at");
            return user;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("UserRepository::findByEmail error: {}", e.what());
    }
    
    return std::nullopt;
}

bool UserRepository::create(const User& user) {
    LOG_INFO("UserRepository::create: {}", user.email);
    
    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return false;
    }
    
    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return false;
    }
    
    try {
        std::string query = "INSERT INTO users (id, email, password_hash, nickname, role, status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, user.id);
        stmt->setString(2, user.email);
        stmt->setString(3, user.passwordHash);
        stmt->setString(4, user.nickname);
        stmt->setString(5, user.role);
        stmt->setString(6, user.status == UserStatus::Active ? "active" : "banned");
        stmt->setInt64(7, user.createdAt);
        stmt->setInt64(8, user.updatedAt);
        
        return stmt->executeUpdate() > 0;
    } catch (const std::exception& e) {
        LOG_ERROR("UserRepository::create error: {}", e.what());
    }
    
    return false;
}

bool UserRepository::update(const User& user) {
    LOG_INFO("UserRepository::update: {}", user.id);
    
    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return false;
    }
    
    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return false;
    }
    
    try {
        std::string query = "UPDATE users SET email = ?, password_hash = ?, nickname = ?, role = ?, status = ?, updated_at = ?, last_login_at = ? WHERE id = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, user.email);
        stmt->setString(2, user.passwordHash);
        stmt->setString(3, user.nickname);
        stmt->setString(4, user.role);
        stmt->setString(5, user.status == UserStatus::Active ? "active" : "banned");
        stmt->setInt64(6, user.updatedAt);
        stmt->setInt64(7, user.lastLoginAt);
        stmt->setString(8, user.id);
        
        return stmt->executeUpdate() > 0;
    } catch (const std::exception& e) {
        LOG_ERROR("UserRepository::update error: {}", e.what());
    }
    
    return false;
}

bool UserRepository::remove(const std::string& id) {
    LOG_INFO("UserRepository::remove: {}", id);
    
    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return false;
    }
    
    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return false;
    }
    
    try {
        std::string query = "DELETE FROM users WHERE id = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, id);
        
        return stmt->executeUpdate() > 0;
    } catch (const std::exception& e) {
        LOG_ERROR("UserRepository::remove error: {}", e.what());
    }
    
    return false;
}

std::vector<User> UserRepository::findAll(int offset, int limit) {
    LOG_INFO("UserRepository::findAll: offset={}, limit={}", offset, limit);
    
    std::vector<User> users;
    
    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return users;
    }
    
    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return users;
    }
    
    try {
        std::string query = "SELECT id, email, nickname, role, status, created_at FROM users LIMIT ? OFFSET ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setInt(1, limit);
        stmt->setInt(2, offset);
        
        auto result = stmt->executeQuery();
        while (result->next()) {
            User user;
            user.id = result->getString("id");
            user.email = result->getString("email");
            user.nickname = result->getString("nickname");
            user.role = result->getString("role");
            user.status = result->getString("status") == "active" ? UserStatus::Active : UserStatus::Banned;
            user.createdAt = result->getInt64("created_at");
            users.push_back(user);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("UserRepository::findAll error: {}", e.what());
    }
    
    return users;
}

bool UserRepository::save(const User& user) {
    LOG_INFO("UserRepository::save: {}", user.email);
    
    auto existing = findById(user.id);
    if (existing) {
        return update(user);
    }
    return create(user);
}

bool UserRepository::updateNickname(const std::string& userId, const std::string& nickname) {
    LOG_INFO("UserRepository::updateNickname: {}", userId);
    
    if (!pool_) {
        LOG_ERROR("Connection pool is null");
        return false;
    }
    
    auto conn = pool_->acquire();
    if (!conn) {
        LOG_ERROR("Failed to get connection");
        return false;
    }
    
    try {
        std::string query = "UPDATE users SET nickname = ?, updated_at = ? WHERE id = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, nickname);
        stmt->setInt64(2, std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        stmt->setString(3, userId);
        
        return stmt->executeUpdate() > 0;
    } catch (const std::exception& e) {
        LOG_ERROR("UserRepository::updateNickname error: {}", e.what());
    }
    
    return false;
}
