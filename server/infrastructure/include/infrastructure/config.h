#pragma once

#include <string>
#include <cstdint>

struct DatabaseConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "root";
    std::string password = "";
    std::string database = "ai_chat";
    size_t min_connections = 5;
    size_t max_connections = 20;
};

struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password = "";
    int db = 0;
};

struct ServerConfig {
    std::string http_addr = "0.0.0.0";
    uint16_t http_port = 8080;
    uint16_t ws_port = 8081;
    std::string cert_file = "cert.pem";
    std::string key_file = "key.pem";
    size_t thread_pool_size = 8;
};

struct AiConfig {
    std::string endpoint = "https://api.openai.com/v1/chat/completions";
    std::string api_key = "";
    std::string model = "gpt-3.5-turbo";
    int max_tokens = 2048;
    int timeout_seconds = 30;
};

struct EmailConfig {
    std::string smtp_server;
    int smtp_port = 587;
    std::string username;
    std::string password;
    std::string from_address;
};

class Config {
public:
    static Config& instance();
    bool load(const std::string& filepath);

    std::string log_level;
    std::string log_file;
    DatabaseConfig database;
    RedisConfig redis;
    ServerConfig server;
    AiConfig ai;
    EmailConfig email;

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};