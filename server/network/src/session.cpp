#include "network/session.h"
#include "network/ws_server.h"
#include "infrastructure/logger.h"
#include "infrastructure/token_manager.h"
#include "infrastructure/crypto_util.h"
#include "business/service_locator.h"
#include "business/chat_service.h"
#include "data/redis_cache.h"
#include <nlohmann/json.hpp>
#include <chrono>

Session::Session(tcp::socket socket, WsServer* server)
    : ws_(std::move(socket)), server_(server) {}

Session::~Session() = default;

void Session::run() {
    do_accept();
}

void Session::do_accept() {
    auto self(shared_from_this());

    http::async_read(ws_.next_layer(), buffer_, upgrade_req_,
        [self](boost::system::error_code ec, size_t) {
            if (ec) {
                LOG_ERROR("WebSocket read upgrade error: {}", ec.message());
                return;
            }

            std::string target(self->upgrade_req_.target().data(), self->upgrade_req_.target().size());
            size_t pos = target.find("token=");
            if (pos == std::string::npos) {
                LOG_ERROR("WebSocket: no token in URL");
                return;
            }

            std::string token = target.substr(pos + 6);
            size_t amp = token.find('&');
            if (amp != std::string::npos) {
                token = token.substr(0, amp);
            }

            auto token_mgr = ServiceLocator::instance().tokenManager();
            if (!token_mgr) {
                LOG_ERROR("WebSocket: token manager not available");
                return;
            }

            auto payload = token_mgr->validateToken(token);
            if (!payload) {
                LOG_ERROR("WebSocket: invalid token");
                return;
            }

            auto redis = ServiceLocator::instance().redisCache();
            if (redis) {
                std::string stored_token = redis->get("token:" + payload->user_id);
                if (!stored_token.empty() && stored_token != token) {
                    LOG_ERROR("WebSocket: token mismatch for user {}", payload->user_id);
                    return;
                }
            }

            self->user_id_ = payload->user_id;

            self->ws_.async_accept(self->upgrade_req_,
                [self](boost::system::error_code ec) {
                    if (ec) {
                        LOG_ERROR("WebSocket accept error: {}", ec.message());
                        return;
                    }

                    self->server_->registerSession(self->user_id_, self);

                    LOG_INFO("WebSocket session started for user: {}", self->user_id_);
                    self->do_read();
                });
        });
}

void Session::do_read() {
    auto self(shared_from_this());
    ws_.async_read(buffer_,
        [self](boost::system::error_code ec, size_t bytes_transferred) {
            self->on_read(ec, bytes_transferred);
        });
}

void Session::on_read(boost::system::error_code ec, size_t) {
    if (ec) {
        if (!closing_) {
            LOG_INFO("WebSocket read closed for user {}: {}", user_id_, ec.message());
            server_->unregisterSession(user_id_);
        }
        return;
    }

    std::string message = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    handleMessage(message);

    if (!closing_) {
        do_read();
    }
}

void Session::handleMessage(const std::string& message) {
    try {
        auto j = nlohmann::json::parse(message);
        std::string type = j.value("type", "");

        if (type == "chat_message") {
            std::string content = j.value("data", nlohmann::json::object()).value("content", "");
            LOG_INFO("Chat message from user {}: {}", user_id_, content);

            auto& chat_service = ServiceLocator::instance().getChatService();
            std::string session_id = chat_service.createSession(user_id_);
            auto result = chat_service.sendMessage(session_id, user_id_, content);

            nlohmann::json resp = {
                {"type", "chat_response"},
                {"data", {
                    {"content", result["content"]},
                    {"id", result["id"]}
                }}
            };
            send(resp.dump());
        } else if (type == "ping") {
            send("{\"type\":\"pong\",\"data\":{}}");
        } else {
            send("{\"type\":\"error\",\"data\":{\"message\":\"Unknown message type\"}}");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("WebSocket parse error: {}", e.what());
        send("{\"type\":\"error\",\"data\":{\"message\":\"Invalid message format\"}}");
    }
}

void Session::send(const std::string& message) {
    auto self(shared_from_this());
    write_buffer_ = message;
    ws_.async_write(net::buffer(write_buffer_),
        [self](boost::system::error_code ec, size_t) {
            if (ec) {
                LOG_ERROR("WebSocket write error for user {}: {}", self->user_id_, ec.message());
            }
        });
}

void Session::close() {
    closing_ = true;
    auto self(shared_from_this());
    boost::system::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
}
