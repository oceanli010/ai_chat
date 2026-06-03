#include "PasswordHasher.h"

// generate_hash_and_salt
// 功能：生成 32 字节随机盐值，结合密码使用 PBKDF2-HMAC-SHA1 派生密钥
// 参数：password - 明文密码
//       out_hash - 输出参数，派生密钥的十六进制字符串
//       out_salt - 输出参数，随机盐值的十六进制字符串
// 返回值：无
void PasswordHasher::generate_hash_and_salt(const std::string& password,
                                             std::string& out_hash,
                                             std::string& out_salt) {
    unsigned char salt_bytes[32];
    RAND_bytes(salt_bytes, sizeof(salt_bytes));

    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(salt_bytes[i]);
    }
    out_salt = ss.str();

    out_hash = hash_password(password, out_salt);
}

// verify_password
// 功能：验证密码是否匹配存储的哈希
// 参数：password - 待验证的明文密码
//       salt - 存储的盐值
//       expected_hash - 存储的期望哈希值
// 返回值：bool - true 表示密码匹配
// 说明：使用 PBKDF2 比对密码哈希
bool PasswordHasher::verify_password(const std::string& password,
                                      const std::string& salt,
                                      const std::string& expected_hash) {
    std::string computed = hash_password(password, salt);
    return computed == expected_hash;
}

// hash_password
// 功能：使用 PBKDF2-HMAC-SHA1 对密码加盐迭代 100000 次
// 参数：password - 明文密码
//       salt - 盐值字符串（十六进制）
// 返回值：string - 20 字节派生密钥的十六进制字符串
std::string PasswordHasher::hash_password(const std::string& password,
                                           const std::string& salt) {
    unsigned char derived_key[20];
    PKCS5_PBKDF2_HMAC_SHA1(
        password.c_str(), static_cast<int>(password.length()),
        reinterpret_cast<const unsigned char*>(salt.c_str()),
        static_cast<int>(salt.length()),
        100000,
        sizeof(derived_key), derived_key);

    std::stringstream ss;
    for (int i = 0; i < sizeof(derived_key); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(derived_key[i]);
    }
    return ss.str();
}
