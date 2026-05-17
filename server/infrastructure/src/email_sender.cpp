#include "infrastructure/email_sender.h"
#include "infrastructure/config.h"
#include "infrastructure/logger.h"
#include <curl/curl.h>
#include <sstream>

struct EmailBuffer {
    std::string data;
    size_t pos = 0;
};

static size_t readEmailBody(void* ptr, size_t size, size_t nmemb, void* userp) {
    auto* buf = static_cast<EmailBuffer*>(userp);
    if (buf->pos >= buf->data.size()) return 0;

    size_t chunk = size * nmemb;
    size_t remaining = buf->data.size() - buf->pos;
    size_t to_copy = std::min(chunk, remaining);

    memcpy(ptr, buf->data.data() + buf->pos, to_copy);
    buf->pos += to_copy;
    return to_copy;
}

bool EmailSender::send(const std::string& to, const std::string& subject, const std::string& body) {
    auto& cfg = Config::instance().email;
    if (cfg.smtp_server.empty() || cfg.username.empty() || cfg.password.empty()) {
        LOG_ERROR("Email config incomplete: smtp_server={}, username={}", cfg.smtp_server, cfg.username);
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize curl");
        return false;
    }

    std::string url = "smtp://" + cfg.smtp_server + ":" + std::to_string(cfg.smtp_port);
    std::string from = "<" + cfg.from_address + ">";
    std::string recipient = "<" + to + ">";

    std::ostringstream oss;
    oss << "From: " << cfg.from_address << "\r\n"
        << "To: " << to << "\r\n"
        << "Subject: " << subject << "\r\n"
        << "MIME-Version: 1.0\r\n"
        << "Content-Type: text/plain; charset=UTF-8\r\n"
        << "Content-Transfer-Encoding: 8bit\r\n"
        << "\r\n"
        << body << "\r\n";

    EmailBuffer buf{oss.str(), 0};

    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, recipient.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readEmailBody);
    curl_easy_setopt(curl, CURLOPT_READDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_USERNAME, cfg.username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, cfg.password.c_str());
    curl_easy_setopt(curl, CURLOPT_LOGIN_OPTIONS, "AUTH=LOGIN");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to send email to {}: {}", to, curl_easy_strerror(res));
        return false;
    }

    LOG_INFO("Email sent successfully to {}: subject={}", to, subject);
    return true;
}
