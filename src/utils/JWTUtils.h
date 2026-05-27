#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class JWTUtils {
public:
    static void init(const std::string& secret);

    static std::string generate_token(uint64_t user_id,
                                       const std::string& username,
                                       const std::string& role);

    struct TokenData {
        uint64_t user_id = 0;
        std::string username;
        std::string role;
    };

    static TokenData verify_token(const std::string& token);

private:
    static std::string secret_;

    static std::string base64url_encode(const std::string& input);
    static std::string base64url_decode(const std::string& input);
    static std::string hmac_sha256(const std::string& data,
                                    const std::string& key);
    static std::vector<std::string> split(const std::string& str,
                                            char delimiter);
};
