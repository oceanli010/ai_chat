#pragma once

#include <string>
#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class WsServer;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, WsServer* server);
    ~Session();

    void run();
    void send(const std::string& message);
    void close();

    std::string userId() const { return user_id_; }

private:
    void do_accept();
    void do_read();
    void on_read(boost::system::error_code ec, size_t bytes_transferred);
    void handleMessage(const std::string& message);
    std::string generateResponse(const std::string& content);

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> upgrade_req_;
    WsServer* server_;
    std::string user_id_;
    std::string write_buffer_;
    bool closing_ = false;
};
