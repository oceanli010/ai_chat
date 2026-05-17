#pragma once

#include <string>

class CryptoUtil {
public:
    static std::string sha256(const std::string& input);
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password, const std::string& hash);
    static std::string generateRandomString(size_t length);
    static std::string generateUUID();
    static std::string base64Encode(const std::string& input);
    static std::string base64Decode(const std::string& input);
    static std::string hmacSha256(const std::string& data, const std::string& key);

private:
    CryptoUtil() = default;
};