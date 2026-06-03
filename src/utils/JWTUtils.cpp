#include "JWTUtils.h"

std::string JWTUtils::secret_ = "";

// init
// 功能：设置 JWT 签名密钥
// 参数：secret - HMAC-SHA256 签名用的密钥字符串
// 返回值：无
void JWTUtils::init(const std::string& secret) {
    secret_ = secret;
}

// generate_token
// 功能：生成 JWT Token，有效期 7 天
// 参数：user_id - 用户 ID
//       username - 用户名
//       role - 用户角色
// 返回值：string - JWT 格式的 Token（header.payload.signature）
// 说明：header 使用 HS256 算法，payload 包含 iss/sub/iat/exp/username/role
std::string JWTUtils::generate_token(uint64_t user_id,
                                      const std::string& username,
                                      const std::string& role) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24 * 7); // 过期时间为当前时间 + 7 天
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

// verify_token
// 功能：验证 JWT Token 的签名和有效期
// 参数：token - JWT 字符串
// 返回值：TokenData - 包含 user_id / username / role 的结构体
// 说明：验证失败或 Token 过期时抛 runtime_error 异常
JWTUtils::TokenData JWTUtils::verify_token(const std::string& token) {
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

        // 检查 Token 是否过期
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

// base64url_encode
// 功能：将输入字符串进行 Base64 URL 编码（RFC 4648 §5）
// 参数：input - 待编码的原字符串
// 返回值：string - 编码后的字符串（不含填充字符 =）
// 说明：使用 -_ 替代 +/ 以确保 URL 安全
std::string JWTUtils::base64url_encode(const std::string& input) {
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

// base64url_decode
// 功能：将 Base64 URL 编码的字符串解码为原字符串
// 参数：input - Base64 URL 编码的字符串
// 返回值：string - 解码后的原字符串
// 说明：支持忽略非 Base64 字符
std::string JWTUtils::base64url_decode(const std::string& input) {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    // 构建反向查找表（字符 -> 索引值）
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

    // 每 4 个 6-bit 值解码为 3 个字节
    for (size_t i = 0; i + 3 < buf.size(); i += 4) {
        result += (buf[i] << 2) | (buf[i + 1] >> 4);
        result += (buf[i + 1] << 4) | (buf[i + 2] >> 2);
        result += (buf[i + 2] << 6) | buf[i + 3];
    }

    // 处理剩余字节（不足 4 的倍数）
    if (buf.size() % 4 == 3) {
        result += (buf[buf.size() - 3] << 2) | (buf[buf.size() - 2] >> 4);
        result += (buf[buf.size() - 2] << 4) | (buf[buf.size() - 1] >> 2);
    } else if (buf.size() % 4 == 2) {
        result += (buf[buf.size() - 2] << 2) | (buf[buf.size() - 1] >> 4);
    }

    return result;
}

// hmac_sha256
// 功能：使用 HMAC-SHA256 算法对数据进行签名
// 参数：data - 待签名的数据
//       key - HMAC 密钥
// 返回值：string - 十六进制字符串格式的 HMAC 签名
std::string JWTUtils::hmac_sha256(const std::string& data,
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

// split
// 功能：按指定分隔符拆分字符串
// 参数：str - 待拆分的字符串
//       delimiter - 分隔符字符
// 返回值：vector<string> - 拆分后的子串列表
std::vector<std::string> JWTUtils::split(const std::string& str,
                                           char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(str);
    std::string part;
    while (std::getline(ss, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}
