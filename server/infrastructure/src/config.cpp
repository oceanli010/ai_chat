#include "infrastructure/config.h"
#include <fstream>
#include <nlohmann/json.hpp>

Config& Config::instance() {
    static Config instance;
    return instance;
}

bool Config::load(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            fprintf(stderr, "[ERROR] Failed to open config file: %s\n", filepath.c_str());
            return false;
        }

        nlohmann::json j;
        file >> j;

        log_level = j.value("log_level", "info");
        log_file = j.value("log_file", "logs");

        if (j.contains("database")) {
            auto& db = j["database"];
            database.host = db.value("host", "127.0.0.1");
            database.port = db.value("port", 3306);
            database.user = db.value("user", "root");
            database.password = db.value("password", "");
            database.database = db.value("database", "ai_chat");
            database.min_connections = db.value("min_connections", 5);
            database.max_connections = db.value("max_connections", 20);
        }

        if (j.contains("redis")) {
            auto& r = j["redis"];
            redis.host = r.value("host", "127.0.0.1");
            redis.port = r.value("port", 6379);
            redis.password = r.value("password", "");
            redis.db = r.value("db", 0);
        }

        if (j.contains("server")) {
            auto& s = j["server"];
            server.http_addr = s.value("http_addr", "0.0.0.0");
            server.http_port = s.value("http_port", 8080);
            server.ws_port = s.value("ws_port", 8081);
            server.cert_file = s.value("cert_file", "cert.pem");
            server.key_file = s.value("key_file", "key.pem");
            server.thread_pool_size = s.value("thread_pool_size", 8);
        }

        if (j.contains("ai")) {
            auto& a = j["ai"];
            ai.endpoint = a.value("endpoint", "https://api.openai.com/v1/chat/completions");
            ai.api_key = a.value("api_key", "");
            ai.model = a.value("model", "gpt-3.5-turbo");
            ai.max_tokens = a.value("max_tokens", 2048);
            ai.timeout_seconds = a.value("timeout_seconds", 30);
        }

        if (j.contains("email")) {
            auto& e = j["email"];
            email.smtp_server = e.value("smtp_server", "");
            email.smtp_port = e.value("smtp_port", 587);
            email.username = e.value("username", "");
            email.password = e.value("password", "");
            email.from_address = e.value("from_address", "");
        }

        fprintf(stdout, "[INFO] Configuration loaded successfully from: %s\n", filepath.c_str());
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "[ERROR] Failed to parse config file: %s\n", e.what());
        return false;
    }
}

// Config destructor: no cleanup needed
