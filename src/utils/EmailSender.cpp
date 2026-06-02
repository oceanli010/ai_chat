#include "EmailSender.h"
#include "utils/Logger.h"
#include <sstream>
#include <iomanip>

// ============================================================
// 构造 / 析构
// ============================================================

EmailSender::EmailSender(const std::string& smtp_host,
                         int smtp_port,
                         const std::string& username,
                         const std::string& password,
                         const std::string& from_address)
    : smtp_host_(smtp_host)
    , smtp_port_(smtp_port)
    , username_(username)
    , password_(password)
    , from_address_(from_address)
{}

// ============================================================
// 高层发送接口
// ============================================================

bool EmailSender::send_verification_code(const std::string& to_email,
                                          const std::string& code) {
    std::string subject = "AI聊天室 - 邮箱验证码";
    std::string body =
        "您的验证码是: " + code + "\n"
        "验证码 5 分钟内有效。\n\n"
        "如果您未请求此验证码，请忽略此邮件。";

    return send_email(to_email, subject, body);
}

bool EmailSender::send_delete_account_code(const std::string& to_email,
                                            const std::string& code) {
    std::string subject = "AI聊天室 - 账号注销验证";
    std::string body =
        "您正在申请注销账号，此操作将删除所有数据且不可恢复！\n\n"
        "您的验证码是: " + code + "\n"
        "验证码 10 分钟内有效。\n\n"
        "如非本人操作，请忽略此邮件并立即修改密码。";

    return send_email(to_email, subject, body);
}

bool EmailSender::send_account_deleted_notification(const std::string& to_email,
                                                     const std::string& admin_name) {
    std::string subject = "AI聊天室 - 账号已被管理员注销";
    std::string body =
        "您好，\n\n"
        "您的账号已被管理员 (" + admin_name + ") 强制注销。\n"
        "注销时间: " + get_current_time_string() + "\n\n"
        "如您对此操作有疑问，请联系管理员进行申诉。\n\n"
        "感谢您的使用。";

    return send_email(to_email, subject, body);
}

// ============================================================
// 核心发送逻辑（libcurl SMTP）
// ============================================================

bool EmailSender::send_email(const std::string& to,
                              const std::string& subject,
                              const std::string& body) {
    try {
        // ---- RAII 创建 CURL 句柄 ----
        CurlHandle curl;

        // ---- RAII 创建收件人列表 ----
        CurlSlist recipients;
        recipients.append(to);

        // ---- 构造邮件 payload ----
        std::string payload = build_mail_payload(from_address_, to, subject, body);
        PayloadContext ctx(std::move(payload));

        // ---- 详细错误信息缓冲区 ----
        char errbuf[CURL_ERROR_SIZE] = {0};

        // ---- 配置 SMTP（端口 465→smtps:// 隐式TLS，端口 587→smtp://+STARTTLS） ----
        bool use_implicit_tls = (smtp_port_ == 465);
        std::string protocol = use_implicit_tls ? "smtps://" : "smtp://";

        std::string url = protocol + smtp_host_ + ":" + std::to_string(smtp_port_);

        curl_easy_setopt(curl, CURLOPT_URL,             url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME,        username_.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD,        password_.c_str());
        curl_easy_setopt(curl, CURLOPT_LOGIN_OPTIONS,   "AUTH=LOGIN");
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM,       from_address_.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT,       recipients.get());
        curl_easy_setopt(curl, CURLOPT_READFUNCTION,    payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA,        &ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD,          1L);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER,     errbuf);

        // 关闭 libcurl verbose 输出
        curl_easy_setopt(curl, CURLOPT_VERBOSE,         0L);

        // 连接 / 传输超时（秒）
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,  15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,         30L);

        // 非隐式 TLS 端口需要显式启用 STARTTLS
        if (!use_implicit_tls) {
            curl_easy_setopt(curl, CURLOPT_USE_SSL,     CURLUSESSL_ALL);
        }

        // 关闭证书严格验证
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,  0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,  0L);

        APP_LOG_INFO("Sending email to {} via {}…", to, url);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            APP_LOG_ERROR("Email send failed for {}: {} (detail: {})",
                          to, curl_easy_strerror(res),
                          errbuf[0] ? errbuf : "no server message");
            return false;
        }

        APP_LOG_INFO("Email sent successfully to {}", to);
        return true;

    } catch (const std::exception& e) {
        APP_LOG_ERROR("Email send exception: {}", e.what());
        return false;
    }
}

// ============================================================
// libcurl CURLOPT_READFUNCTION 回调
// ============================================================

size_t EmailSender::payload_source(char* ptr, size_t size, size_t nmemb,
                                    void* userdata) {
    auto* ctx = static_cast<PayloadContext*>(userdata);
    if (!ctx) return 0;

    size_t room  = size * nmemb;
    size_t remain = ctx->data.size() - ctx->offset;
    size_t to_copy = (remain < room) ? remain : room;

    if (to_copy > 0) {
        memcpy(ptr, ctx->data.data() + ctx->offset, to_copy);
        ctx->offset += to_copy;
    }
    return to_copy;
}

// ============================================================
// 构造 RFC 2822 邮件 payload
// ============================================================

std::string EmailSender::build_mail_payload(const std::string& from,
                                             const std::string& to,
                                             const std::string& subject,
                                             const std::string& body) {
    std::ostringstream oss;
    oss << "From: " << from << "\r\n"
        << "To: " << to << "\r\n"
        << "Subject: " << subject << "\r\n"
        << "Date: " << get_current_time_string() << "\r\n"
        << "MIME-Version: 1.0\r\n"
        << "Content-Type: text/plain; charset=UTF-8\r\n"
        << "Content-Transfer-Encoding: 8bit\r\n"
        << "Message-ID: <" << std::chrono::system_clock::now().time_since_epoch().count()
        << "@ai_chat>\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string EmailSender::get_current_time_string() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&t);   // SMTP 日期建议用 GMT
    char buf[128];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S +0000", &tm);
    return std::string(buf);
}
