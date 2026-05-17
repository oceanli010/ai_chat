#include "network/http_server.h"
#include "infrastructure/logger.h"
#include "infrastructure/token_manager.h"
#include "business/service_locator.h"
#include "business/auth_service.h"
#include "business/user_service.h"
#include "business/admin_service.h"
#include "business/chat_service.h"

using namespace std;

static nlohmann::json handle_request(const string& path, const string& method, const string& body, const string& token) {
    LOG_DEBUG("HTTP request: {} {}", method, path);

    try {
        if (path == "/api/auth/register" && method == "POST") {
            auto data = nlohmann::json::parse(body);
            auto& user_service = ServiceLocator::instance().getUserService();
            return user_service.sendVerificationCode(data["email"]);
        } else if (path == "/api/auth/verify" && method == "POST") {
            auto data = nlohmann::json::parse(body);
            auto& user_service = ServiceLocator::instance().getUserService();
            return user_service.registerUser(data["email"], data["code"], data["nickname"], data["password"]);
        } else if (path == "/api/auth/login" && method == "POST") {
            auto data = nlohmann::json::parse(body);
            auto& auth_service = ServiceLocator::instance().getAuthService();
            return auth_service.login(data["email"], data["password"]);
        } else if (path == "/api/auth/logout" && method == "POST") {
            auto& auth_service = ServiceLocator::instance().getAuthService();
            return {{"status", auth_service.logout(token) ? "ok" : "error"}};
        } else if (path == "/api/user/profile" && method == "GET") {
            auto& user_service = ServiceLocator::instance().getUserService();
            return user_service.getProfile(token);
        } else if (path == "/api/user/nickname" && method == "PUT") {
            auto data = nlohmann::json::parse(body);
            auto& user_service = ServiceLocator::instance().getUserService();
            return user_service.updateNickname(token, data["nickname"]);
        } else if (path == "/api/user/account" && method == "DELETE") {
            auto& user_service = ServiceLocator::instance().getUserService();
            return user_service.deleteAccount(token);
        } else if (path.starts_with("/api/admin/users") && method == "GET") {
            auto& admin_service = ServiceLocator::instance().getAdminService();
            return admin_service.getUsers(token, 1, 20, "");
        } else if (path == "/api/admin/ban" && method == "POST") {
            auto data = nlohmann::json::parse(body);
            auto& admin_service = ServiceLocator::instance().getAdminService();
            return admin_service.banUser(token, data["user_id"], data["ban"]);
        } else if (path == "/api/admin/stats" && method == "GET") {
            auto& admin_service = ServiceLocator::instance().getAdminService();
            return admin_service.getStats(token);
        } else if (path.starts_with("/api/chat/history") && method == "GET") {
            auto token_mgr = ServiceLocator::instance().tokenManager();
            if (!token_mgr) {
                return {{"status", "error"}, {"message", "Service not available"}};
            }
            auto payload = token_mgr->validateToken(token);
            if (!payload) {
                return {{"status", "error"}, {"message", "Invalid token"}};
            }

            int page = 1, size = 50;
            size_t qpos = path.find('?');
            if (qpos != string::npos) {
                string query = path.substr(qpos + 1);
                size_t ppos = query.find("page=");
                if (ppos != string::npos) {
                    page = stoi(query.substr(ppos + 5));
                }
                size_t spos = query.find("size=");
                if (spos != string::npos) {
                    size = stoi(query.substr(spos + 5));
                }
            }

            auto& chat_service = ServiceLocator::instance().getChatService();
            auto messages = chat_service.getHistory(payload->user_id, size, (page - 1) * size);
            nlohmann::json result;
            result["status"] = "ok";
            result["messages"] = messages;
            return result;
        } else if (path == "/api/chat/history" && method == "DELETE") {
            auto token_mgr = ServiceLocator::instance().tokenManager();
            if (!token_mgr) {
                return {{"status", "error"}, {"message", "Service not available"}};
            }
            auto payload = token_mgr->validateToken(token);
            if (!payload) {
                return {{"status", "error"}, {"message", "Invalid token"}};
            }

            auto& chat_service = ServiceLocator::instance().getChatService();
            bool ok = chat_service.clearHistory(payload->user_id);
            return {{"status", ok ? "ok" : "error"}};
        }
        
        return {{"status", "error"}, {"message", "Endpoint not found"}};
    } catch (const exception& e) {
        LOG_ERROR("HTTP request error: {}", e.what());
        return {{"status", "error"}, {"message", e.what()}};
    }
}

static string extract_token(const http::request<http::string_body>& req) {
    auto auth_header = req[http::field::authorization];
    if (auth_header.empty()) return "";
    string auth(auth_header.data(), auth_header.size());
    if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ") {
        return auth.substr(7);
    }
    return "";
}

struct HttpSession : enable_shared_from_this<HttpSession> {
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;

    explicit HttpSession(tcp::socket socket) : socket_(move(socket)) {}

    void start() { do_read(); }

private:
    void do_read() {
        auto self(shared_from_this());
        http::async_read(socket_, buffer_, req_,
            [this, self](boost::system::error_code ec, size_t) {
                if (!ec) do_write(handle_request(
                    string(req_.target().data(), req_.target().size()),
                    string(req_.method_string().data(), req_.method_string().size()),
                    req_.body(),
                    extract_token(req_)
                ));
            });
    }

    void do_write(nlohmann::json response) {
        auto self(shared_from_this());
        auto res = make_shared<http::response<http::string_body>>();
        res->version(req_.version());
        res->keep_alive(req_.keep_alive());
        res->result(http::status::ok);
        res->set(http::field::content_type, "application/json");
        res->set(http::field::access_control_allow_origin, "*");
        res->set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
        res->set(http::field::access_control_allow_headers, "Content-Type, Authorization");
        res->body() = response.dump();
        res->prepare_payload();

        http::async_write(socket_, *res,
            [this, self, res](boost::system::error_code, size_t) {
                socket_.shutdown(tcp::socket::shutdown_send);
            });
    }
};

HttpServer::HttpServer(net::ip::address addr, uint16_t port, shared_ptr<ThreadPool>)
    : acceptor_(ioc_, tcp::endpoint(addr, port)) {}

HttpServer::~HttpServer() { stop(); }

void HttpServer::run() {
    running_ = true;
    io_thread_ = thread([this]() {
        acceptor_.listen();
        do_accept();
        ioc_.run();
    });
    LOG_INFO("HTTP server running on {}:{}", 
        acceptor_.local_endpoint().address().to_string(),
        acceptor_.local_endpoint().port());
}

void HttpServer::stop() {
    running_ = false;
    ioc_.stop();
    if (io_thread_.joinable()) io_thread_.join();
    LOG_INFO("HTTP server stopped");
}

void HttpServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec && running_) {
                make_shared<HttpSession>(move(socket))->start();
            }
            if (running_) do_accept();
        });
}
