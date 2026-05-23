#pragma once

#include <string>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>

class PasswordHasher {
public:
    static void generate_hash_and_salt(const std::string& password,
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

    static bool verify_password(const std::string& password,
                                 const std::string& salt,
                                 const std::string& expected_hash) {
        std::string computed = hash_password(password, salt);
        return computed == expected_hash;
    }

private:
    static std::string hash_password(const std::string& password,
                                      const std::string& salt) {
        std::string salted = password + salt;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, salted.c_str(), salted.length());
        SHA256_Final(hash, &sha256);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(hash[i]);
        }
        return ss.str();
    }
};
