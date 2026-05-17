#include "infrastructure/crypto_util.h"
#include "infrastructure/logger.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iomanip>
#include <sstream>
#include <cstring>

static const int PBKDF2_ITERATIONS = 10000;
static const int PBKDF2_KEY_LENGTH = 32;
static const int PBKDF2_SALT_LENGTH = 16;

std::string CryptoUtil::sha256(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, input.data(), input.size());
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string CryptoUtil::hashPassword(const std::string& password) {
    unsigned char salt[PBKDF2_SALT_LENGTH];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        LOG_ERROR("Failed to generate random salt");
        return "";
    }

    unsigned char derived[PBKDF2_KEY_LENGTH];
    int rc = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.size()),
        salt, sizeof(salt),
        PBKDF2_ITERATIONS,
        EVP_sha256(),
        PBKDF2_KEY_LENGTH,
        derived);
    if (rc != 1) {
        LOG_ERROR("PBKDF2 key derivation failed");
        return "";
    }

    std::string salt_b64 = base64Encode(std::string(reinterpret_cast<char*>(salt), sizeof(salt)));
    std::string hash_b64 = base64Encode(std::string(reinterpret_cast<char*>(derived), sizeof(derived)));

    return "$pbkdf2-sha256$" + std::to_string(PBKDF2_ITERATIONS) + "$" + salt_b64 + "$" + hash_b64;
}

bool CryptoUtil::verifyPassword(const std::string& password, const std::string& hash) {
    const std::string prefix = "$pbkdf2-sha256$";
    if (hash.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    size_t pos1 = prefix.size();
    size_t pos2 = hash.find('$', pos1);
    if (pos2 == std::string::npos) return false;

    int iterations = std::stoi(hash.substr(pos1, pos2 - pos1));
    size_t pos3 = hash.find('$', pos2 + 1);
    if (pos3 == std::string::npos) return false;

    std::string salt_b64 = hash.substr(pos2 + 1, pos3 - pos2 - 1);
    std::string hash_b64 = hash.substr(pos3 + 1);

    std::string salt = base64Decode(salt_b64);
    std::string expected = base64Decode(hash_b64);

    unsigned char derived[PBKDF2_KEY_LENGTH];
    int rc = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.size()),
        reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
        iterations,
        EVP_sha256(),
        PBKDF2_KEY_LENGTH,
        derived);
    if (rc != 1) return false;

    return std::memcmp(derived, expected.data(), PBKDF2_KEY_LENGTH) == 0;
}

std::string CryptoUtil::generateRandomString(size_t length) {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = sizeof(charset) - 2;

    std::vector<unsigned char> buf(length);
    if (RAND_bytes(buf.data(), static_cast<int>(length)) != 1) {
        LOG_ERROR("Failed to generate random bytes");
        return "";
    }

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[buf[i] % max_index];
    }
    return result;
}

std::string CryptoUtil::generateUUID() {
    boost::uuids::random_generator generator;
    boost::uuids::uuid uuid = generator();
    return boost::uuids::to_string(uuid);
}

std::string CryptoUtil::base64Encode(const std::string& input) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    BIO_write(bio, input.data(), static_cast<int>(input.size()));
    BIO_flush(bio);

    BUF_MEM* buf_mem = nullptr;
    BIO_get_mem_ptr(bio, &buf_mem);

    std::string result(buf_mem->data, buf_mem->length);
    BIO_free_all(bio);
    return result;
}

std::string CryptoUtil::base64Decode(const std::string& input) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new_mem_buf(input.data(), static_cast<int>(input.size()));
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<char> buf(input.size());
    int len = BIO_read(bio, buf.data(), static_cast<int>(input.size()));
    BIO_free_all(bio);

    if (len < 0) return "";
    return std::string(buf.data(), len);
}

std::string CryptoUtil::hmacSha256(const std::string& data, const std::string& key) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;

    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         result, &result_len);

    std::ostringstream oss;
    for (unsigned int i = 0; i < result_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result[i]);
    }
    return oss.str();
}