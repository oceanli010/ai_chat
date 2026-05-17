#include <gtest/gtest.h>
#include "infrastructure/crypto_util.h"

TEST(CryptoUtilTest, Sha256) {
    std::string hash = CryptoUtil::sha256("hello");
    EXPECT_EQ(hash.length(), 64);
    EXPECT_NE(hash, "");
}

TEST(CryptoUtilTest, HashAndVerifyPassword) {
    std::string password = "test_password_123";
    std::string hash = CryptoUtil::hashPassword(password);
    EXPECT_FALSE(hash.empty());
    EXPECT_TRUE(hash.find("$pbkdf2-sha256$10000$") != std::string::npos);
    EXPECT_TRUE(CryptoUtil::verifyPassword(password, hash));
    EXPECT_FALSE(CryptoUtil::verifyPassword("wrong_password", hash));
}

TEST(CryptoUtilTest, GenerateRandomString) {
    std::string s1 = CryptoUtil::generateRandomString(32);
    std::string s2 = CryptoUtil::generateRandomString(32);
    EXPECT_EQ(s1.length(), 32);
    EXPECT_EQ(s2.length(), 32);
    EXPECT_NE(s1, s2);
}

TEST(CryptoUtilTest, GenerateUUID) {
    std::string uuid = CryptoUtil::generateUUID();
    EXPECT_FALSE(uuid.empty());
    EXPECT_GE(uuid.length(), 36);
}

TEST(CryptoUtilTest, Base64EncodeDecode) {
    std::string original = "Hello, World!";
    std::string encoded = CryptoUtil::base64Encode(original);
    std::string decoded = CryptoUtil::base64Decode(encoded);
    EXPECT_EQ(original, decoded);
}

TEST(CryptoUtilTest, HmacSha256) {
    std::string hmac = CryptoUtil::hmacSha256("message", "key");
    EXPECT_EQ(hmac.length(), 64);
}