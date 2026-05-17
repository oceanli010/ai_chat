#include "network/ws_server.h"
#include "network/session.h"
#include "infrastructure/logger.h"

WsServer::WsServer(net::ip::address addr, uint16_t port, std::shared_ptr<ThreadPool> pool)
    : acceptor_(ioc_, net::ip::tcp::endpoint(addr, port)), thread_pool_(std::move(pool)) {}

WsServer::~WsServer() { stop(); }

void WsServer::run() {
    running_ = true;
    io_thread_ = std::thread([this]() {
        acceptor_.listen(net::socket_base::max_listen_connections);
        do_accept();
        ioc_.run();
    });
    LOG_INFO("WebSocket server running on {}:{}",
        acceptor_.local_endpoint().address().to_string(),
        acceptor_.local_endpoint().port());
}

void WsServer::stop() {
    running_ = false;
    ioc_.stop();
    if (io_thread_.joinable()) io_thread_.join();
    LOG_INFO("WebSocket server stopped");
}

void WsServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, net::ip::tcp::socket socket) {
            if (!ec && running_) {
                auto session = std::make_shared<Session>(std::move(socket), this);
                session->run();
            }
            if (running_) do_accept();
        });
}

void WsServer::registerSession(const std::string& userId, std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(userId);
    if (it != sessions_.end()) {
        auto old = it->second;
        sessions_.erase(it);
        old->close();
        LOG_INFO("Kicked old session for user: {}", userId);
    }
    sessions_[userId] = session;
}

void WsServer::unregisterSession(const std::string& userId) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(userId);
}
