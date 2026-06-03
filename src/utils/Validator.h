#pragma once

#include <string>
#include <regex>

// Validator
// 功能：输入校验工具类，提供邮箱、用户名、密码和验证码的格式验证
class Validator {
public:
    // is_valid_email
    // 功能：验证邮箱格式是否合法
    // 参数：email - 待验证的邮箱地址字符串
    // 返回值：bool - true 表示格式合法
    // 说明：匹配标准邮箱格式，如 user@example.com
    static bool is_valid_email(const std::string& email) {
        static const std::regex pattern(
            R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
        return std::regex_match(email, pattern);
    }

    // is_valid_username
    // 功能：验证用户名格式是否合法
    // 参数：username - 待验证的用户名字符串
    // 返回值：bool - true 表示格式合法
    // 说明：长度为 3~50 个字符，只能包含字母、数字和下划线
    static bool is_valid_username(const std::string& username) {
        if (username.length() < 3 || username.length() > 50) {
            return false;
        }
        static const std::regex pattern(R"(^[a-zA-Z0-9_]+$)");
        return std::regex_match(username, pattern);
    }

    // is_valid_password
    // 功能：验证密码长度是否合法
    // 参数：password - 待验证的密码字符串
    // 返回值：bool - true 表示密码长度在 6~128 字符之间
    static bool is_valid_password(const std::string& password) {
        return password.length() >= 6 && password.length() <= 128;
    }

    // is_valid_verification_code
    // 功能：验证邮箱验证码是否为 6 位数字
    // 参数：code - 待验证的验证码字符串
    // 返回值：bool - true 表示是有效的 6 位数字验证码
    static bool is_valid_verification_code(const std::string& code) {
        if (code.length() != 6) return false;
        for (char c : code) {
            if (!isdigit(c)) return false;
        }
        return true;
    }
};
