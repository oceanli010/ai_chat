#include "EmailSender.h"
#include <openssl/err.h>

// OpenSSL 全局初始化，只执行一次
namespace {
    class OpenSSLGlobalInit {
    public:
        OpenSSLGlobalInit() {
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
        }
    };
    OpenSSLGlobalInit openssl_init;
}

EmailSender::EmailSender(const std::string& smtp_host,
                         int smtp_port,
                         const std::string& username,
                         const std::string& password,
                         const std::string& from_address)
    : smtp_host_(smtp_host)
    , smtp_port_(smtp_port)
    , username_(username)
    , password_(password)
    , from_address_(from_address) {}

bool EmailSender::send_verification_code(const std::string& to_email,
                                          const std::string& code) {
    std::string subject = "AI聊天室 - 邮箱验证码";
    std::string body = "您的验证码是: " + code +
                       "\n验证码5分钟内有效。\n\n"
                       "如果您未请求此验证码，请忽略此邮件。";

    return send_email(to_email, subject, body);
}

bool EmailSender::send_delete_account_code(const std::string& to_email,
                                            const std::string& code) {
    std::string subject = "AI聊天室 - 账号注销验证";
    std::string body = "您正在申请注销账号，此操作将删除所有数据且不可恢复！\n\n"
                       "您的验证码是: " + code +
                       "\n验证码10分钟内有效。\n\n"
                       "如非本人操作，请忽略此邮件并立即修改密码。";

    return send_email(to_email, subject, body);
}

bool EmailSender::send_account_deleted_notification(const std::string& to_email,
                                                     const std::string& admin_name) {
    std::string subject = "AI聊天室 - 账号已被管理员注销";
    std::string body = "您好，\n\n"
                       "您的账号已被管理员(" + admin_name + ")强制注销。\n"
                       "注销时间: " + getCurrentTimeString() + "\n\n"
                       "如您对此操作有疑问，请联系管理员进行申诉。\n\n"
                       "感谢您的使用。";

    return send_email(to_email, subject, body);
}

bool EmailSender::send_email(const std::string& to,
                              const std::string& subject,
                              const std::string& body) {
    // SSL 全局初始化已在静态对象中完成，此处不再重复调用

    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        APP_LOG_ERROR("Failed to create SSL context");
        return false;
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

    SSL_set_tlsext_host_name(ssl, smtp_host_.c_str());

    std::string hostport = smtp_host_ + ":" + std::to_string(smtp_port_);
    BIO_set_conn_hostname(bio, hostport.c_str());

    if (BIO_do_connect(bio) <= 0) {
        APP_LOG_ERROR("Failed to connect to SMTP server");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    APP_LOG_INFO("Sending email to {}...", to);

    auto read_line = [bio]() -> std::string {
        std::string line;
        char c;
        while (BIO_read(bio, &c, 1) > 0) {
            line += c;
            if (c == '\n') break;
        }
        return line;
    };

    auto send_cmd = [bio](const std::string& cmd) -> bool {
        return BIO_write(bio, cmd.c_str(), cmd.length()) > 0;
    };

    auto expect_code = [&](const std::string& expected) -> bool {
        std::string resp = read_line();
        return resp.size() >= 3 && resp.substr(0, 3) == expected;
    };

    if (!expect_code("220")) {
        APP_LOG_ERROR("SMTP greeting failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    if (!send_cmd("EHLO localhost\r\n")) {
        APP_LOG_ERROR("EHLO send failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    // EHLO 响应可能是多行的（例如 250-SIZE...、250-AUTH...），需要全部读取
    {
        bool ehlo_ok = false;
        std::string line;
        do {
            line = read_line();
            if (line.size() >= 3) {
                std::string code = line.substr(0, 3);
                if (code == "250" && line.size() >= 4 && line[3] == ' ') {
                    ehlo_ok = true;
                    break;
                } else if (code != "250") {
                    // 非 250 开头，EHLO 失败
                    break;
                }
                // 250- 继续读取下一行
            }
        } while (!line.empty());
        if (!ehlo_ok) {
            APP_LOG_ERROR("EHLO failed");
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }
    }

    if (!send_cmd("AUTH LOGIN\r\n") || !expect_code("334")) {
        APP_LOG_ERROR("AUTH LOGIN initiation failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    if (!send_cmd(base64_encode(username_) + "\r\n") || !expect_code("334")) {
        APP_LOG_ERROR("AUTH LOGIN username failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    if (!send_cmd(base64_encode(password_) + "\r\n") || !expect_code("235")) {
        APP_LOG_ERROR("AUTH LOGIN password failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    if (!send_cmd("MAIL FROM:<" + from_address_ + ">\r\n") || !expect_code("250")) {
        APP_LOG_ERROR("MAIL FROM failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    if (!send_cmd("RCPT TO:<" + to + ">\r\n") || !expect_code("250")) {
        APP_LOG_ERROR("RCPT TO failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    if (!send_cmd("DATA\r\n") || !expect_code("354")) {
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
        body + "\r\n.\r\n";

    if (!send_cmd(content) || !expect_code("250")) {
        APP_LOG_ERROR("DATA content failed");
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    send_cmd("QUIT\r\n");

    APP_LOG_INFO("Email sent successfully to {}", to);

    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    return true;
}

std::string EmailSender::base64_encode(const std::string& input) {
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

std::string EmailSender::getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}
