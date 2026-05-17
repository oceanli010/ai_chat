#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

class ThreadPool;

class HttpServer {
public:
    HttpServer(net::ip::address addr, uint16_t port, std::shared_ptr<ThreadPool> pool);
    ~HttpServer();

    void run();
    void stop();

private:
    void do_accept();

    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<ThreadPool> thread_pool_;
    std::atomic<bool> running_{false};
    std::thread io_thread_;
};
