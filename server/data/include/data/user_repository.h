#pragma once

#include "model/user.h"
#include "data/connection_pool.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>

class UserRepository {
public:
    explicit UserRepository(std::shared_ptr<ConnectionPool> pool);

    std::optional<User> findById(const std::string& id);
    std::optional<User> findByEmail(const std::string& email);
    bool create(const User& user);
    bool update(const User& user);
    bool remove(const std::string& id);
    std::vector<User> findAll(int offset, int limit);
    bool save(const User& user);
    bool updateNickname(const std::string& userId, const std::string& nickname);
    bool updatePassword(const std::string& userId, const std::string& newPasswordHash);

private:
    std::shared_ptr<ConnectionPool> pool_;
};