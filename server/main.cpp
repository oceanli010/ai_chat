#include "infrastructure/logger.h"
#include "infrastructure/config.h"
#include "infrastructure/thread_pool.h"
#include "infrastructure/token_manager.h"
#include "data/connection_pool.h"
#include "data/redis_cache.h"
#include "data/user_repository.h"
#include "data/chat_repository.h"
#include "business/service_locator.h"
#include "network/http_server.h"
#include "network/ws_server.h"

#include <boost/asio/ip/address.hpp>
#include <csignal>
#include <memory>
#include <atomic>

static std::shared_ptr<HttpServer> http_server;
static std::shared_ptr<WsServer> ws_server;
static std::atomic<bool> running{true};

static void signalHandler(int signal) {
    if (running.exchange(false)) {
        if (Logger::instance().get()) {
            LOG_INFO("Received signal {}, shutting down...", signal);
        } else {
            fprintf(stdout, "[INFO] Received signal %d, shutting down...\n", signal);
        }
        if (http_server) http_server->stop();
        if (ws_server) ws_server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string config_path = "config/server.conf";
    if (argc > 1) {
        config_path = argv[1];
    }

    auto& config = Config::instance();
    if (!config.load(config_path)) {
        fprintf(stderr, "Failed to load config from %s\n", config_path.c_str());
        return 1;
    }

    spdlog::level::level_enum log_level = spdlog::level::from_str(config.log_level);
    Logger::instance().init(config.log_file, log_level);

    LOG_INFO("=== ai_chat server starting (v1.0.0) ===");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto thread_pool = std::make_shared<ThreadPool>(config.server.thread_pool_size);

    auto conn_pool = std::make_shared<ConnectionPool>(
        config.database.host, config.database.port,
        config.database.user, config.database.password,
        config.database.database,
        config.database.min_connections, config.database.max_connections);

    auto redis = std::make_shared<RedisCache>(
        config.redis.host, config.redis.port,
        config.redis.password, config.redis.db);

    auto token_mgr = std::make_shared<TokenManager>("ai_chat_secret_key_change_me");

    auto user_repo = std::make_shared<UserRepository>(conn_pool);
    auto chat_repo = std::make_shared<ChatRepository>(conn_pool);

    auto& locator = ServiceLocator::instance();
    locator.setConnectionPool(conn_pool);
    locator.setRedisCache(redis);
    locator.setTokenManager(token_mgr);
    locator.setUserRepository(user_repo);
    locator.setChatRepository(chat_repo);

    if (redis) {
        redis->connect();
    }

    auto http_addr = boost::asio::ip::make_address(config.server.http_addr);
    http_server = std::make_shared<HttpServer>(http_addr, config.server.http_port, thread_pool);
    http_server->run();

    ws_server = std::make_shared<WsServer>(http_addr, config.server.ws_port, thread_pool);
    ws_server->run();

    LOG_INFO("Server initialized successfully");
    LOG_INFO("  HTTP on {}:{}", config.server.http_addr, config.server.http_port);
    LOG_INFO("  WS on {}:{}", config.server.http_addr, config.server.ws_port);

    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    LOG_INFO("Server shutdown complete");
    return 0;
}