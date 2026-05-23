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
    static void init(const std::string& secret) {
        secret_ = secret;
    }

    static std::string generate_token(uint64_t user_id,
                                       const std::string& username,
                                       const std::string& role) {
        auto now = std::chrono::system_clock::now();
        auto exp = now + std::chrono::hours(24 * 7);
        auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        auto exp_ts = std::chrono::duration_cast<std::chrono::seconds>(
            exp.time_since_epoch()).count();

        json header = {{"alg", "HS256"}, {"typ", "JWT"}};
        json payload = {
            {"iss", "ai_chat"},
            {"sub", std::to_string(user_id)},
            {"iat", now_ts},
            {"exp", exp_ts},
            {"username", username},
            {"role", role}
        };

        std::string header_b64 = base64url_encode(header.dump());
        std::string payload_b64 = base64url_encode(payload.dump());
        std::string signing_input = header_b64 + "." + payload_b64;
        std::string signature = hmac_sha256(signing_input, secret_);

        return signing_input + "." + signature;
    }

    struct TokenData {
        uint64_t user_id = 0;
        std::string username;
        std::string role;
    };

    static TokenData verify_token(const std::string& token) {
        TokenData data;

        auto parts = split(token, '.');
        if (parts.size() != 3) {
            throw std::runtime_error("Invalid token format");
        }

        std::string signing_input = parts[0] + "." + parts[1];
        std::string expected_sig = hmac_sha256(signing_input, secret_);

        if (expected_sig != parts[2]) {
            throw std::runtime_error("Invalid signature");
        }

        std::string payload_str = base64url_decode(parts[1]);

        try {
            json payload = json::parse(payload_str);

            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (payload.contains("exp") && payload["exp"].get<int64_t>() < now) {
                throw std::runtime_error("Token expired");
            }

            data.user_id = std::stoull(payload["sub"].get<std::string>());
            data.username = payload["username"].get<std::string>();
            data.role = payload["role"].get<std::string>();
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid token payload: " + std::string(e.what()));
        }

        return data;
    }

private:
    static std::string secret_;

    static std::string base64url_encode(const std::string& input) {
        static const char* chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

        std::string result;
        size_t len = input.length();
        const unsigned char* data =
            reinterpret_cast<const unsigned char*>(input.data());

        for (size_t i = 0; i < len; i += 3) {
            unsigned char b0 = data[i];
            unsigned char b1 = (i + 1 < len) ? data[i + 1] : 0;
            unsigned char b2 = (i + 2 < len) ? data[i + 2] : 0;

            result += chars[b0 >> 2];
            result += chars[((b0 & 0x03) << 4) | (b1 >> 4)];
            if (i + 1 < len) {
                result += chars[((b1 & 0x0F) << 2) | (b2 >> 6)];
            }
            if (i + 2 < len) {
                result += chars[b2 & 0x3F];
            }
        }

        return result;
    }

    static std::string base64url_decode(const std::string& input) {
        static const std::string chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

        std::vector<int> table(256, -1);
        for (size_t i = 0; i < 64; ++i) {
            table[static_cast<unsigned char>(chars[i])] = i;
        }

        std::string result;
        size_t len = input.length();
        std::vector<unsigned char> buf;
        for (size_t i = 0; i < len; ++i) {
            unsigned char c = input[i];
            if (table[c] != -1) {
                buf.push_back(table[c]);
            }
        }

        for (size_t i = 0; i + 3 < buf.size(); i += 4) {
            result += (buf[i] << 2) | (buf[i + 1] >> 4);
            result += (buf[i + 1] << 4) | (buf[i + 2] >> 2);
            result += (buf[i + 2] << 6) | buf[i + 3];
        }

        if (buf.size() % 4 == 3) {
            result += (buf[buf.size() - 3] << 2) | (buf[buf.size() - 2] >> 4);
        } else if (buf.size() % 4 == 2) {
            result += (buf[buf.size() - 2] << 2) | (buf[buf.size() - 1] >> 4);
            result += (buf[buf.size() - 1] << 4) | (buf[buf.size() - 0] >> 2);
            result.pop_back();
        }

        return result;
    }

    static std::string hmac_sha256(const std::string& data,
                                    const std::string& key) {
        unsigned char result[EVP_MAX_MD_SIZE];
        unsigned int result_len = 0;

        HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
             reinterpret_cast<const unsigned char*>(data.c_str()),
             data.length(), result, &result_len);

        static const char* hex_chars = "0123456789abcdef";
        std::string sig;
        for (unsigned int i = 0; i < result_len; ++i) {
            sig += hex_chars[(result[i] >> 4) & 0x0F];
            sig += hex_chars[result[i] & 0x0F];
        }
        return sig;
    }

    static std::vector<std::string> split(const std::string& str,
                                            char delimiter) {
        std::vector<std::string> parts;
        std::stringstream ss(str);
        std::string part;
        while (std::getline(ss, part, delimiter)) {
            parts.push_back(part);
        }
        return parts;
    }
};
