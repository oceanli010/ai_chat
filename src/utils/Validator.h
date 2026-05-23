#pragma once

#include <string>
#include <regex>

class Validator {
public:
    static bool is_valid_email(const std::string& email) {
        static const std::regex pattern(
            R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
        return std::regex_match(email, pattern);
    }

    static bool is_valid_username(const std::string& username) {
        if (username.length() < 3 || username.length() > 50) {
            return false;
        }
        static const std::regex pattern(R"(^[a-zA-Z0-9_]+$)");
        return std::regex_match(username, pattern);
    }

    static bool is_valid_password(const std::string& password) {
        return password.length() >= 6 && password.length() <= 128;
    }

    static bool is_valid_verification_code(const std::string& code) {
        if (code.length() != 6) return false;
        for (char c : code) {
            if (!isdigit(c)) return false;
        }
        return true;
    }
};
