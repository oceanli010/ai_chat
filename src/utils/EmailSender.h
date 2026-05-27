#pragma once

#include <string>
#include <mutex>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include "utils/Logger.h"

class EmailSender {
public:
    EmailSender(const std::string& smtp_host,
                int smtp_port,
                const std::string& username,
                const std::string& password,
                const std::string& from_address);

    bool send_verification_code(const std::string& to_email,
                                 const std::string& code);

    bool send_delete_account_code(const std::string& to_email,
                                   const std::string& code);

    bool send_account_deleted_notification(const std::string& to_email,
                                            const std::string& admin_name);

private:
    bool send_email(const std::string& to,
                     const std::string& subject,
                     const std::string& body);

    static std::string base64_encode(const std::string& input);
    static std::string getCurrentTimeString();

    std::string smtp_host_;
    int smtp_port_;
    std::string username_;
    std::string password_;
    std::string from_address_;
    std::mutex mutex_;
};
