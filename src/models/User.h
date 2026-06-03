#pragma once

#include <string>
#include <cstdint>

// User
// 用户数据模型，对应数据库中 users 表的字段结构
struct User {
    uint64_t id = 0;               // 用户唯一标识
    std::string username;          // 用户名
    std::string password_hash;     // 密码哈希值
    std::string password_salt;     // 密码加盐
    std::string email;             // 电子邮箱
    std::string nickname;          // 用户昵称
    std::string role = "user";     // 用户角色（user / admin）
    std::string status = "active"; // 账号状态（active / pending_deletion / banned）
    uint32_t total_chats = 0;      // 总对话次数
    std::string created_at;        // 创建时间
    std::string updated_at;        // 更新时间
    std::string deleted_at;        // 软删除时间
};
