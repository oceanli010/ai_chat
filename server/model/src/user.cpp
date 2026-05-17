#include "model/user.h"

std::string userStatusToString(UserStatus s) {
    switch (s) {
        case UserStatus::Active: return "active";
        case UserStatus::Banned: return "banned";
    }
    return "unknown";
}

nlohmann::json User::toJson() const {
    return {
        {"id", id},
        {"email", email},
        {"nickname", nickname},
        {"role", role},
        {"status", userStatusToString(status)},
        {"created_at", createdAt},
        {"updated_at", updatedAt},
        {"last_login_at", lastLoginAt}
    };
}

User User::fromJson(const nlohmann::json& j) {
    User u;
    u.id = j.value("id", "");
    u.email = j.value("email", "");
    u.passwordHash = j.value("password_hash", "");
    u.nickname = j.value("nickname", "");
    u.role = j.value("role", "user");
    u.createdAt = j.value("created_at", 0);
    u.updatedAt = j.value("updated_at", 0);
    u.lastLoginAt = j.value("last_login_at", 0);

    std::string status_str = j.value("status", "active");
    if (status_str == "banned") {
        u.status = UserStatus::Banned;
    } else {
        u.status = UserStatus::Active;
    }

    return u;
}