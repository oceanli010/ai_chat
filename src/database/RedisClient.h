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
              int db = 0) {
        host_ = host;
        port_ = port;
        password_ = password;
        db_ = db;

        context_ = redisConnect(host.c_str(), port);
        if (!context_ || context_->err) {
            APP_LOG_ERROR("Redis connection failed: {}",
                      context_ ? context_->errstr : "null context");
            if (context_) {
                redisFree(context_);
                context_ = nullptr;
            }
            return;
        }

        if (!password_.empty()) {
            auto* reply = (redisReply*)redisCommand(context_, "AUTH %s",
                                                     password_.c_str());
            if (!reply || reply->type == REDIS_REPLY_ERROR) {
                APP_LOG_ERROR("Redis auth failed");
            }
            freeReplyObject(reply);
        }

        if (db_ > 0) {
            auto* reply = (redisReply*)redisCommand(context_, "SELECT %d", db_);
            freeReplyObject(reply);
        }

        APP_LOG_INFO("Redis connected to {}:{}", host_, port_);
    }

    bool set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "SET %s %s",
                                                 key.c_str(), value.c_str());
        bool ok = reply && reply->type == REDIS_REPLY_STATUS &&
                  std::string(reply->str) == "OK";
        freeReplyObject(reply);
        return ok;
    }

    bool setex(const std::string& key, int seconds, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "SETEX %s %d %s",
                                                 key.c_str(), seconds,
                                                 value.c_str());
        bool ok = reply && reply->type == REDIS_REPLY_STATUS &&
                  std::string(reply->str) == "OK";
        freeReplyObject(reply);
        return ok;
    }

    std::string get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "GET %s",
                                                 key.c_str());
        std::string result;
        if (reply && reply->type == REDIS_REPLY_STRING) {
            result = reply->str;
        }
        freeReplyObject(reply);
        return result;
    }

    bool del(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "DEL %s", key.c_str());
        bool ok = reply && reply->type == REDIS_REPLY_INTEGER;
        freeReplyObject(reply);
        return ok;
    }

    long long incr(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "INCR %s", key.c_str());
        long long val = 0;
        if (reply && reply->type == REDIS_REPLY_INTEGER) {
            val = reply->integer;
        }
        freeReplyObject(reply);
        return val;
    }

    std::vector<std::string> smembers(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "SMEMBERS %s", key.c_str());
        std::vector<std::string> result;
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < reply->elements; ++i) {
                if (reply->element[i]->type == REDIS_REPLY_STRING) {
                    result.push_back(reply->element[i]->str);
                }
            }
        }
        freeReplyObject(reply);
        return result;
    }

    long long sadd(const std::string& key, const std::string& member) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "SADD %s %s",
                                                 key.c_str(), member.c_str());
        long long val = 0;
        if (reply && reply->type == REDIS_REPLY_INTEGER) {
            val = reply->integer;
        }
        freeReplyObject(reply);
        return val;
    }

    long long srem(const std::string& key, const std::string& member) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "SREM %s %s",
                                                 key.c_str(), member.c_str());
        long long val = 0;
        if (reply && reply->type == REDIS_REPLY_INTEGER) {
            val = reply->integer;
        }
        freeReplyObject(reply);
        return val;
    }

    long long scard(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "SCARD %s", key.c_str());
        long long val = 0;
        if (reply && reply->type == REDIS_REPLY_INTEGER) {
            val = reply->integer;
        }
        freeReplyObject(reply);
        return val;
    }

    bool sismember(const std::string& key, const std::string& member) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "SISMEMBER %s %s",
                                                 key.c_str(), member.c_str());
        bool result = reply && reply->type == REDIS_REPLY_INTEGER &&
                      reply->integer == 1;
        freeReplyObject(reply);
        return result;
    }

    bool exists(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "EXISTS %s", key.c_str());
        bool result = reply && reply->type == REDIS_REPLY_INTEGER &&
                      reply->integer == 1;
        freeReplyObject(reply);
        return result;
    }

    bool expire(const std::string& key, int seconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "EXPIRE %s %d",
                                                 key.c_str(), seconds);
        bool ok = reply && reply->type == REDIS_REPLY_INTEGER &&
                  reply->integer == 1;
        freeReplyObject(reply);
        return ok;
    }

    long long ttl(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* reply = (redisReply*)redisCommand(context_, "TTL %s", key.c_str());
        long long val = -1;
        if (reply && reply->type == REDIS_REPLY_INTEGER) {
            val = reply->integer;
        }
        freeReplyObject(reply);
        return val;
    }

    ~RedisClient() {
        if (context_) {
            redisFree(context_);
        }
    }

private:
    RedisClient() : context_(nullptr), port_(6379), db_(0) {}

    redisContext* context_;
    std::string host_;
    int port_;
    std::string password_;
    int db_;
    std::mutex mutex_;
};
