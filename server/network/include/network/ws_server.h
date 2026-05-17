#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <boost/asio/ip/tcp.hpp>

namespace net = boost::asio;

class ThreadPool;
class Session;

class WsServer {
public:
    WsServer(net::ip::address addr, uint16_t port, std::shared_ptr<ThreadPool> pool);
    ~WsServer();

    void run();
    void stop();

    void registerSession(const std::string& userId, std::shared_ptr<Session> session);
    void unregisterSession(const std::string& userId);

private:
    void do_accept();

    net::io_context ioc_;
    net::ip::tcp::acceptor acceptor_;
    std::shared_ptr<ThreadPool> thread_pool_;
    std::atomic<bool> running_{false};
    std::thread io_thread_;

    std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};
