#pragma once

#include <string>
#include <cstdio>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <ctime>
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
        , from_address_(from_address) {}

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
        char temp_file[] = "/tmp/ai_chat_email_XXXXXX";
        int fd = mkstemp(temp_file);
        if (fd < 0) {
            APP_LOG_ERROR("Failed to create temp file for email");
            return false;
        }

        std::string content =
            "From: \"" + from_address_ + "\" <" + from_address_ + ">\r\n" +
            "To: <" + to + ">\r\n" +
            "Subject: =?UTF-8?B?" + base64_encode(subject) + "?=\r\n" +
            "MIME-Version: 1.0\r\n" +
            "Content-Type: text/plain; charset=UTF-8\r\n" +
            "Content-Transfer-Encoding: 8bit\r\n" +
            "\r\n" +
            body + "\r\n";

        write(fd, content.c_str(), content.length());
        close(fd);

        std::string cmd =
            "curl -s --ssl-reqd --max-time 30 "
            "--login-options AUTH=LOGIN "
            "--url 'smtps://" + smtp_host_ + ":" + std::to_string(smtp_port_) + "' "
            "--user '" + escape_shell(username_) + ":" + escape_shell(password_) + "' "
            "--mail-from '" + escape_shell(from_address_) + "' "
            "--mail-rcpt '" + escape_shell(to) + "' "
            "--upload-file '" + std::string(temp_file) + "' 2>&1";

        APP_LOG_INFO("Sending email to {}...", to);

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            APP_LOG_ERROR("Failed to execute curl for email");
            unlink(temp_file);
            return false;
        }

        char buf[1024];
        std::string output;
        while (fgets(buf, sizeof(buf), pipe)) {
            output += buf;
        }
        int rc = pclose(pipe);
        unlink(temp_file);

        if (rc == 0) {
            APP_LOG_INFO("Email sent successfully to {}", to);
            return true;
        } else {
            APP_LOG_ERROR("Email send failed: {}", output);
            return false;
        }
    }

    static std::string escape_shell(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\'') result += "'\\''";
            else result += c;
        }
        return result;
    }

    static std::string base64_encode(const std::string& input) {
        static const char* chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string result;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        int in_len = input.length();
        const char* bytes_to_encode = input.c_str();

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
            for (j = i; j < 3; j++) {
                char_array_3[j] = '\0';
            }
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            for (j = 0; j < i + 1; j++) {
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