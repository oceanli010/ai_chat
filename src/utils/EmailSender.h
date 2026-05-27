#pragma once

#include <string>
#include <mutex>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "utils/Logger.h"

class EmailSender {
public:
    EmailSender(const std::string& smtp_host,
                int smtp_port,
                const std::string& username,
                const std::string& password,
                const std::string& from_address)
        : smtp_host_(smtp_host)
        , smtp_port_(smtp_port)
        , username_(username)
        , password_(password)
        , from_address_(from_address) {
        static std::once_flag ssl_init_flag;
        std::call_once(ssl_init_flag, []() {
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
        });
    }

    bool send_verification_code(const std::string& to_email,
                                 const std::string& code) {
        std::string subject = "AI聊天室 - 邮箱验证码";
        std::string body = "您的验证码是: " + code +
                           "\n验证码5分钟内有效。\n\n"
                           "如果您未请求此验证码，请忽略此邮件。";

        return send_email(to_email, subject, body);
    }

    bool send_delete_account_code(const std::string& to_email,
                                   const std::string& code) {
        std::string subject = "AI聊天室 - 账号注销验证";
        std::string body = "您正在申请注销账号，此操作将删除所有数据且不可恢复！\n\n"
                           "您的验证码是: " + code +
                           "\n验证码10分钟内有效。\n\n"
                           "如非本人操作，请忽略此邮件并立即修改密码。";

        return send_email(to_email, subject, body);
    }

    bool send_account_deleted_notification(const std::string& to_email,
                                            const std::string& admin_name) {
        std::string subject = "AI聊天室 - 账号已被管理员注销";
        std::string body = "您好，\n\n"
                           "您的账号已被管理员(" + admin_name + ")强制注销。\n"
                           "注销时间: " + getCurrentTimeString() + "\n\n"
                           "如您对此操作有疑问，请联系管理员进行申诉。\n\n"
                           "感谢您的使用。";

        return send_email(to_email, subject, body);
    }

private:
    bool send_email(const std::string& to,
                     const std::string& subject,
                     const std::string& body) {
        SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
        if (!ctx) {
            APP_LOG_ERROR("Failed to create SSL context");
            return false;
        }

        SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

        if (smtp_port_ == 465) {
            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
        }

        BIO* bio = BIO_new_ssl_connect(ctx);
        if (!bio) {
            APP_LOG_ERROR("Failed to create SSL BIO");
            SSL_CTX_free(ctx);
            return false;
        }

        SSL* ssl = nullptr;
        BIO_get_ssl(bio, &ssl);
        if (!ssl) {
            APP_LOG_ERROR("Failed to get SSL object");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

        if (!smtp_host_.empty()) {
            SSL_set_tlsext_host_name(ssl, smtp_host_.c_str());
        }

        std::string hostport = smtp_host_ + ":" + std::to_string(smtp_port_);
        BIO_set_conn_hostname(bio, hostport.c_str());

        if (BIO_do_connect(bio) <= 0) {
            APP_LOG_ERROR("Failed to connect to SMTP server {}:{}", smtp_host_, smtp_port_);
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        APP_LOG_INFO("Sending email to {}...", to);

        auto read_line = [bio]() -> std::string {
            std::string line;
            line.reserve(256);
            char c;
            while (line.size() < 8192) {
                int ret = BIO_read(bio, &c, 1);
                if (ret <= 0) {
                    if (BIO_should_retry(bio)) continue;
                    break;
                }
                line += c;
                if (c == '\n') break;
            }
            return line;
        };

        auto send_data = [bio](const std::string& data) -> bool {
            size_t sent = 0;
            while (sent < data.size()) {
                int ret = BIO_write(bio, data.data() + sent, static_cast<int>(data.size() - sent));
                if (ret <= 0) {
                    if (BIO_should_retry(bio)) continue;
                    return false;
                }
                sent += static_cast<size_t>(ret);
            }
            BIO_flush(bio);
            return true;
        };

        auto read_expected = [&](const std::string& expected) -> bool {
            std::string resp;
            while (true) {
                resp = read_line();
                if (resp.empty()) return false;

                if (resp.size() >= 4 && resp[3] == '-') {
                    continue;
                }
                return resp.size() >= 3 && resp.substr(0, 3) == expected;
            }
        };

        if (!read_expected("220")) {
            APP_LOG_ERROR("SMTP greeting failed or timed out");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        if (!send_data("EHLO ai_chat_server\r\n") || !read_expected("250")) {
            APP_LOG_ERROR("EHLO failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        if (!send_data("AUTH LOGIN\r\n") || !read_expected("334")) {
            APP_LOG_ERROR("AUTH LOGIN initiation failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        if (!send_data(base64_encode(username_) + "\r\n") || !read_expected("334")) {
            APP_LOG_ERROR("AUTH LOGIN username failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        if (!send_data(base64_encode(password_) + "\r\n") || !read_expected("235")) {
            APP_LOG_ERROR("AUTH LOGIN password failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        if (!send_data("MAIL FROM:<" + from_address_ + ">\r\n") || !read_expected("250")) {
            APP_LOG_ERROR("MAIL FROM failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        if (!send_data("RCPT TO:<" + to + ">\r\n") || !read_expected("250")) {
            APP_LOG_ERROR("RCPT TO failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        if (!send_data("DATA\r\n") || !read_expected("354")) {
            APP_LOG_ERROR("DATA command failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        std::string content =
            "From: \"" + from_address_ + "\" <" + from_address_ + ">\r\n"
            "To: <" + to + ">\r\n"
            "Subject: =?UTF-8?B?" + base64_encode(subject) + "?=\r\n"
            "MIME-Version: 1.0\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Transfer-Encoding: 8bit\r\n"
            "\r\n" +
            body + "\r\n";

        if (!send_data(content) || !send_data("\r\n.\r\n") || !read_expected("250")) {
            APP_LOG_ERROR("DATA content failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        send_data("QUIT\r\n");

        APP_LOG_INFO("Email sent successfully to {}", to);

        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return true;
    }

    static std::string base64_encode(const std::string& input) {
        static const char* chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string result;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        int in_len = static_cast<int>(input.length());
        const unsigned char* bytes_to_encode =
            reinterpret_cast<const unsigned char*>(input.data());
        int i = 0;

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                                  ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                                  ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;
                for (i = 0; i < 4; i++) {
                    result += chars[char_array_4[i]];
                }
                i = 0;
            }
        }

        if (i) {
            for (int j = i; j < 3; j++) {
                char_array_3[j] = '\0';
            }
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            for (int j = 0; j < i + 1; j++) {
                result += chars[char_array_4[j]];
            }
            while (i++ < 3) {
                result += '=';
            }
        }
        return result;
    }

    std::string smtp_host_;
    int smtp_port_;
    std::string username_;
    std::string password_;
    std::string from_address_;
    std::mutex mutex_;

    static std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return std::string(buf);
    }
};
