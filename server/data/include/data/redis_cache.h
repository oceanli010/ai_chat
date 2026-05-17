#pragma once

#include <string>
#include <hiredis/hiredis.h>
#include <memory>

class RedisCache {
public:
    RedisCache(const std::string& host, int port, const std::string& password, int db);
    ~RedisCache();

    bool connect();
    void disconnect();
    bool isConnected() const;

    bool set(const std::string& key, const std::string& value);
    bool setEx(const std::string& key, int ttl_seconds, const std::string& value);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);

private:
    redisContext* context_ = nullptr;
    std::string host_;
    int port_;
    std::string password_;
    int db_;
    bool connected_ = false;
};