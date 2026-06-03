#include "EmailSender.h"
#include "utils/Logger.h"
#include <sstream>
#include <iomanip>

// ============================================================
// 构造 / 析构
// ============================================================

// EmailSender（构造函数）
// 功能：初始化 SMTP 邮件发送器，保存服务器连接参数
// 参数：smtp_host - SMTP 服务器主机名
//       smtp_port - SMTP 服务器端口
//       username - SMTP 认证用户名
//       password - SMTP 认证密码
//       from_address - 发件人邮箱地址
// 返回值：无
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

// send_verification_code
// 功能：发送邮箱验证码邮件，验证码 5 分钟有效
// 参数：to_email - 收件人邮箱地址
//       code - 6 位数字验证码
// 返回值：bool - true 表示发送成功
bool EmailSender::send_verification_code(const std::string& to_email,
                                          const std::string& code) {
    std::string subject = "AI聊天室 - 邮箱验证码";
    std::string body =
        "您的验证码是: " + code + "\n"
        "验证码 5 分钟内有效。\n\n"
        "如果您未请求此验证码，请忽略此邮件。";

    return send_email(to_email, subject, body);
}

// send_delete_account_code
// 功能：发送账号注销验证码邮件，验证码 10 分钟有效
// 参数：to_email - 收件人邮箱地址
//       code - 6 位数字验证码
// 返回值：bool - true 表示发送成功
// 说明：邮件内容包含注销风险提示
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

// send_account_deleted_notification
// 功能：管理员强制注销账号后发送通知邮件
// 参数：to_email - 收件人邮箱地址
//       admin_name - 执行注销的管理员用户名
// 返回值：bool - true 表示发送成功
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

// send_email
// 功能：通过 libcurl SMTP API 发送符合 RFC 2822 的邮件
// 参数：to - 收件人邮箱地址
//       subject - 邮件主题
//       body - 邮件正文
// 返回值：bool - true 表示发送成功
// 说明：端口 465 使用隐式 TLS（smtps://），端口 587 使用 STARTTLS
bool EmailSender::send_email(const std::string& to,
                              const std::string& subject,
                              const std::string& body) {
    try {
        // RAII 创建 CURL 句柄
        CurlHandle curl;

        // RAII 创建收件人列表
        CurlSlist recipients;
        recipients.append(to);

        // 构造邮件 payload
        std::string payload = build_mail_payload(from_address_, to, subject, body);
        PayloadContext ctx(std::move(payload));

        // 详细错误信息缓冲区
        char errbuf[CURL_ERROR_SIZE] = {0};

        // 配置 SMTP 协议：端口 465 使用 smtps://（隐式 TLS），端口 587 使用 smtp://+STARTTLS
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

        APP_LOG_INFO("Sending email to {} via {}...", to, url);

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

// payload_source
// 功能：libcurl 回调函数，将邮件 payload 分块提供给 libcurl
// 参数：ptr - 输出缓冲区指针
//       size * nmemb - 缓冲区可写入的字节数
//       userdata - PayloadContext 指针
// 返回值：size_t - 实际写入的字节数，0 表示数据已全部发送
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

// build_mail_payload
// 功能：构造符合 RFC 2822 标准的完整邮件内容（headers + body）
// 参数：from - 发件人地址
//       to - 收件人地址
//       subject - 邮件主题
//       body - 邮件正文（纯文本，UTF-8 编码）
// 返回值：string - 完整的邮件字符串（含 CRLF 行结束符）
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

// get_current_time_string
// 功能：获取当前 GMT 时间的字符串格式，用于 SMTP Date 头
// 参数：无
// 返回值：string - 符合 RFC 2822 格式的时间字符串
// 说明：使用 GMT 时区而非本地时区
std::string EmailSender::get_current_time_string() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&t);   // SMTP 日期建议用 GMT
    char buf[128];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S +0000", &tm);
    return std::string(buf);
}
