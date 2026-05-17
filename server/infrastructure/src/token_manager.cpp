#include "infrastructure/token_manager.h"
#include "infrastructure/crypto_util.h"
#include "infrastructure/logger.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

static const int64_t ACCESS_TOKEN_TTL = 3600;
static const int64_t REFRESH_TOKEN_TTL = 604800;

static std::string base64UrlEncode(const std::string& input) {
    std::string b64 = CryptoUtil::base64Encode(input);
    std::string result;
    for (char c : b64) {
        if (c == '+') result += '-';
        else if (c == '/') result += '_';
        else if (c == '=') break;
        else result += c;
    }
    return result;
}

static std::string base64UrlDecode(const std::string& input) {
    std::string result = input;
    for (char& c : result) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (result.size() % 4 != 0) {
        result += '=';
    }
    return CryptoUtil::base64Decode(result);
}

TokenManager::TokenManager(const std::string& secret)
    : secret_(secret) {}

int64_t TokenManager::now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string TokenManager::createToken(const nlohmann::json& payload) {
    nlohmann::json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };

    std::string header_b64 = base64UrlEncode(header.dump());
    std::string payload_b64 = base64UrlEncode(payload.dump());
    std::string signing_input = header_b64 + "." + payload_b64;

    std::string signature = CryptoUtil::hmacSha256(signing_input, secret_);
    std::string signature_b64 = base64UrlEncode(signature);

    return signing_input + "." + signature_b64;
}

std::string TokenManager::generateAccessToken(const std::string& userId, const std::string& role) {
    nlohmann::json payload = {
        {"user_id", userId},
        {"role", role},
        {"exp", now() + ACCESS_TOKEN_TTL},
        {"jti", CryptoUtil::generateUUID()}
    };
    return createToken(payload);
}

std::string TokenManager::generateRefreshToken(const std::string& userId) {
    nlohmann::json payload = {
        {"user_id", userId},
        {"type", "refresh"},
        {"exp", now() + REFRESH_TOKEN_TTL},
        {"jti", CryptoUtil::generateUUID()}
    };
    return createToken(payload);
}

std::optional<TokenPayload> TokenManager::validateToken(const std::string& token) {
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);

    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return std::nullopt;
    }

    std::string signing_input = token.substr(0, second_dot);
    std::string signature_b64 = token.substr(second_dot + 1);

    std::string expected_signature = CryptoUtil::hmacSha256(signing_input, secret_);
    std::string expected_signature_b64 = base64UrlEncode(expected_signature);

    if (signature_b64 != expected_signature_b64) {
        return std::nullopt;
    }

    std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::string payload_json = base64UrlDecode(payload_b64);

    try {
        nlohmann::json payload = nlohmann::json::parse(payload_json);

        if (!payload.contains("exp") || !payload.contains("user_id")) {
            return std::nullopt;
        }

        int64_t exp = payload["exp"].get<int64_t>();
        if (exp < now()) {
            return std::nullopt;
        }

        TokenPayload result;
        result.user_id = payload["user_id"].get<std::string>();
        result.role = payload.value("role", "user");
        result.exp = exp;
        return result;
    } catch (const std::exception& e) {
        LOG_WARN("Failed to parse token payload: {}", e.what());
        return std::nullopt;
    }
}

void TokenManager::invalidateToken(const std::string& token) {
    std::optional<TokenPayload> payload = validateToken(token);
    if (payload) {
        LOG_INFO("Token invalidated for user: {}", payload->user_id);
    }
}