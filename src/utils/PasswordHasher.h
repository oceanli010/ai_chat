#pragma once

#include <string>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>

// PasswordHasher
// 功能：密码哈希工具类，提供密码加盐哈希与验证
// 说明：使用 PBKDF2-HMAC-SHA1 派生密钥，盐值由安全随机数生成
class PasswordHasher {
public:
    // generate_hash_and_salt
    // 功能：生成随机盐值并计算密码的 PBKDF2 哈希
    // 参数：password - 明文密码
    //       out_hash - 输出参数，计算得到的哈希值（十六进制字符串）
    //       out_salt - 输出参数，生成的随机盐值（十六进制字符串）
    // 返回值：无
    static void generate_hash_and_salt(const std::string& password,
                                        std::string& out_hash,
                                        std::string& out_salt);

    // verify_password
    // 功能：验证密码是否与存储的哈希匹配
    // 参数：password - 待验证的明文密码
    //       salt - 数据库中存储的盐值
    //       expected_hash - 数据库中存储的期望哈希值
    // 返回值：bool - true 表示匹配；否则 false
    // 说明：使用 PBKDF2 比对密码哈希
    static bool verify_password(const std::string& password,
                                 const std::string& salt,
                                 const std::string& expected_hash);

private:
    // hash_password
    // 功能：使用 PBKDF2-HMAC-SHA1 对密码加盐迭代哈希
    // 参数：password - 明文密码
    //       salt - 盐值字符串
    // 返回值：string - 派生密钥的十六进制字符串
    // 说明：迭代次数 100000，输出 20 字节密钥
    static std::string hash_password(const std::string& password,
                                      const std::string& salt);
};
