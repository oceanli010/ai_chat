#pragma once

#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include "utils/Logger.h"

class RedisClient {
public:
    static RedisClient& instance() {
        static RedisClient inst;
        return inst;
    }

    void init(const std::string& host,
              int port,
              const std::string& password = "",
              int db = 0);

    bool set(const std::string& key, const std::string& value);
    bool setex(const std::string& key, int seconds, const std::string& value);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    void del_batch(const std::vector<std::string>& keys);
    long long incr(const std::string& key);

    std::vector<std::string> smembers(const std::string& key);
    long long sadd(const std::string& key, const std::string& member);
    long long srem(const std::string& key, const std::string& member);
    long long scard(const std::string& key);
    bool sismember(const std::string& key, const std::string& member);

    bool exists(const std::string& key);
    bool expire(const std::string& key, int seconds);
    long long ttl(const std::string& key);

    ~RedisClient();

private:
    RedisClient();

    redisContext* context_;
    std::string host_;
    int port_;
    std::string password_;
    int db_;
    std::mutex mutex_;
};
