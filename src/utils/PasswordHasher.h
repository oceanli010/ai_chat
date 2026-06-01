#pragma once

#include <string>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>

class PasswordHasher {
public:
    static void generate_hash_and_salt(const std::string& password,
                                        std::string& out_hash,
                                        std::string& out_salt);

    static bool verify_password(const std::string& password,
                                 const std::string& salt,
                                 const std::string& expected_hash);

private:
    static std::string hash_password(const std::string& password,
                                      const std::string& salt);
};
