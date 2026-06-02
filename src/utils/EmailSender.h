#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <cstring>
#include <curl/curl.h>

// ============================================================
// RAII 封装：CURL 句柄
// ============================================================
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
    CURL* handle_;
};

// ============================================================
// RAII 封装：curl_slist 链表
// ============================================================
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

    void append(const std::string& str) {
        curl_slist* new_list = curl_slist_append(list_, str.c_str());
        if (!new_list) {
            throw std::runtime_error("curl_slist_append failed");
        }
        list_ = new_list;
    }

    curl_slist* get() const { return list_; }

private:
    curl_slist* list_ = nullptr;
};

// ============================================================
// 邮件发送器（基于 libcurl SMTP API）
// ============================================================
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

    // libcurl CURLOPT_READFUNCTION 回调（静态）
    // 将邮件 payload（headers + body）分块传给 libcurl
    static size_t payload_source(char* ptr, size_t size, size_t nmemb, void* userdata);

    // 生成符合 RFC 2822 的完整邮件内容
    static std::string build_mail_payload(const std::string& from,
                                          const std::string& to,
                                          const std::string& subject,
                                          const std::string& body);

    static std::string get_current_time_string();

    std::string smtp_host_;
    int         smtp_port_;
    std::string username_;
    std::string password_;
    std::string from_address_;

    // 用于 payload_source 回调的上下文
    struct PayloadContext {
        const std::string data;
        size_t            offset = 0;
        explicit PayloadContext(std::string d) : data(std::move(d)) {}
    };
};
