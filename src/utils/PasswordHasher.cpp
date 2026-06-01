#include "PasswordHasher.h"

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

bool PasswordHasher::verify_password(const std::string& password,
                                      const std::string& salt,
                                      const std::string& expected_hash) {
    std::string computed = hash_password(password, salt);
    if (computed == expected_hash) {
        return true;
    }
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
    return ss.str() == expected_hash;
}

std::string PasswordHasher::hash_password(const std::string& password,
                                           const std::string& salt) {
    unsigned char derived_key[32];
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
