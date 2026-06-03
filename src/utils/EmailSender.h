#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <cstring>
#include <curl/curl.h>

// CurlHandle
// 功能：RAII 封装 libcurl 的 CURL* 句柄，自动管理资源
// 说明：禁止拷贝，支持移动语义，确保 curl_easy_cleanup 自动调用
class CurlHandle {
public:
    CurlHandle() : handle_(curl_easy_init()) {
        if (!handle_) {
            throw std::runtime_error("curl_easy_init() failed");
        }
    }

    ~CurlHandle() {
        if (handle_) {
            curl_easy_cleanup(handle_);
        }
    }

    CurlHandle(const CurlHandle&)            = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;

    CurlHandle(CurlHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    CurlHandle& operator=(CurlHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                curl_easy_cleanup(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    CURL* get() const { return handle_; }

    operator CURL*() const { return handle_; }

private:
    CURL* handle_; // libcurl easy 句柄指针
};

// CurlSlist
// 功能：RAII 封装 curl_slist 链表，自动管理收发件人列表资源
// 说明：禁止拷贝，支持移动语义，确保 curl_slist_free_all 自动调用
class CurlSlist {
public:
    CurlSlist() = default;

    ~CurlSlist() {
        if (list_) {
            curl_slist_free_all(list_);
        }
    }

    CurlSlist(const CurlSlist&)            = delete;
    CurlSlist& operator=(const CurlSlist&) = delete;

    CurlSlist(CurlSlist&& other) noexcept : list_(other.list_) {
        other.list_ = nullptr;
    }

    CurlSlist& operator=(CurlSlist&& other) noexcept {
        if (this != &other) {
            if (list_) {
                curl_slist_free_all(list_);
            }
            list_ = other.list_;
            other.list_ = nullptr;
        }
        return *this;
    }

    // append
    // 功能：向链表尾部追加一个字符串
    // 参数：str - 待追加的字符串
    // 返回值：无
    // 说明：失败时抛 runtime_error 异常
    void append(const std::string& str) {
        curl_slist* new_list = curl_slist_append(list_, str.c_str());
        if (!new_list) {
            throw std::runtime_error("curl_slist_append failed");
        }
        list_ = new_list;
    }

    curl_slist* get() const { return list_; }

private:
    curl_slist* list_ = nullptr; // curl_slist 链表指针
};

// EmailSender
// 功能：基于 libcurl SMTP API 的邮件发送器
// 说明：支持隐式 TLS（端口 465）和 STARTTLS（端口 587）两种方式
class EmailSender {
public:
    // EmailSender（构造函数）
    // 功能：初始化 SMTP 邮件发送器
    // 参数：smtp_host - SMTP 服务器主机名
    //       smtp_port - SMTP 服务器端口（465 使用隐式 TLS，587 使用 STARTTLS）
    //       username - SMTP 认证用户名
    //       password - SMTP 认证密码
    //       from_address - 发件人邮箱地址
    // 返回值：无
    EmailSender(const std::string& smtp_host,
                int smtp_port,
                const std::string& username,
                const std::string& password,
                const std::string& from_address);

    // send_verification_code
    // 功能：发送邮箱验证码邮件
    // 参数：to_email - 收件人邮箱地址
    //       code - 6 位数字验证码
    // 返回值：bool - true 表示发送成功
    bool send_verification_code(const std::string& to_email,
                                 const std::string& code);

    // send_delete_account_code
    // 功能：发送账号注销验证码邮件
    // 参数：to_email - 收件人邮箱地址
    //       code - 6 位数字验证码
    // 返回值：bool - true 表示发送成功
    bool send_delete_account_code(const std::string& to_email,
                                   const std::string& code);

    // send_account_deleted_notification
    // 功能：管理员注销账号后发送通知邮件
    // 参数：to_email - 收件人邮箱地址
    //       admin_name - 执行注销的管理员用户名
    // 返回值：bool - true 表示发送成功
    bool send_account_deleted_notification(const std::string& to_email,
                                            const std::string& admin_name);

private:
    // send_email
    // 功能：底层邮件发送方法，通过 libcurl SMTP 发送 RFC 2822 格式邮件
    // 参数：to - 收件人邮箱地址
    //       subject - 邮件主题
    //       body - 邮件正文
    // 返回值：bool - true 表示发送成功
    bool send_email(const std::string& to,
                     const std::string& subject,
                     const std::string& body);

    // payload_source
    // 功能：libcurl CURLOPT_READFUNCTION 回调函数
    // 说明：将邮件 payload（headers + body）分块传给 libcurl
    static size_t payload_source(char* ptr, size_t size, size_t nmemb, void* userdata);

    // build_mail_payload
    // 功能：生成符合 RFC 2822 的完整邮件内容
    // 参数：from - 发件人地址
    //       to - 收件人地址
    //       subject - 邮件主题
    //       body - 邮件正文
    // 返回值：string - 完整的 MIME 邮件字符串
    static std::string build_mail_payload(const std::string& from,
                                          const std::string& to,
                                          const std::string& subject,
                                          const std::string& body);

    // get_current_time_string
    // 功能：获取当前 GMT 时间的字符串格式，符合 SMTP 日期规范
    // 参数：无
    // 返回值：string - 形如 "Tue, 01 Jun 2026 12:00:00 +0000" 的时间字符串
    static std::string get_current_time_string();

    std::string smtp_host_;      // SMTP 服务器主机名
    int         smtp_port_;      // SMTP 服务器端口号
    std::string username_;       // SMTP 认证用户名
    std::string password_;       // SMTP 认证密码
    std::string from_address_;   // 发件人邮箱地址

    // PayloadContext
    // 功能：用于 payload_source 回调的上下文
    // 说明：保存待发送的邮件数据和当前读取偏移量
    struct PayloadContext {
        const std::string data;   // 邮件 payload 数据
        size_t            offset = 0; // 当前已发送的字节偏移
        explicit PayloadContext(std::string d) : data(std::move(d)) {}
    };
};
