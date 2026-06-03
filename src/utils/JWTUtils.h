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

// JWTUtils
// 功能：JWT（JSON Web Token）工具类，提供 Token 生成与验证
// 说明：支持 HS256 签名算法，使用 HMAC-SHA256 和 Base64 URL 编码
class JWTUtils {
public:
    // init
    // 功能：初始化 JWT 签名密钥
    // 参数：secret - HMAC 签名密钥字符串
    // 返回值：无
    static void init(const std::string& secret);

    // generate_token
    // 功能：为用户生成 JWT Token，有效期 7 天
    // 参数：user_id - 用户 ID（uint64_t）
    //       username - 用户名
    //       role - 用户角色（如 "admin", "user"）
    // 返回值：string - 生成的 JWT 字符串（header.payload.signature）
    static std::string generate_token(uint64_t user_id,
                                       const std::string& username,
                                       const std::string& role);

    // TokenData
    // 功能：JWT Token 解析后的数据结构体
    struct TokenData {
        uint64_t user_id = 0;   // 用户 ID
        std::string username;   // 用户名
        std::string role;       // 用户角色
    };

    // verify_token
    // 功能：验证 JWT Token 的签名和有效期，解析出 payload 数据
    // 参数：token - JWT 字符串
    // 返回值：TokenData - 解析后的用户信息结构体
    // 说明：验证失败抛 runtime_error 异常（格式错误、签名无效、Token 过期）
    static TokenData verify_token(const std::string& token);

private:
    static std::string secret_; // HMAC 签名密钥

    static std::string base64url_encode(const std::string& input);
    static std::string base64url_decode(const std::string& input);
    static std::string hmac_sha256(const std::string& data,
                                    const std::string& key);
    static std::vector<std::string> split(const std::string& str,
                                            char delimiter);
};
