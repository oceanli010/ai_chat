#include "data/redis_cache.h"
#include "infrastructure/logger.h"

RedisCache::RedisCache(const std::string& host, int port,
                       const std::string& password, int db)
    : host_(host), port_(port), password_(password), db_(db) {}

RedisCache::~RedisCache() {
    disconnect();
}

bool RedisCache::connect() {
    if (connected_) return true;
    context_ = redisConnect(host_.c_str(), port_);
    if (context_ == nullptr || context_->err) {
        LOG_ERROR("Redis connection failed: {}", context_ ? context_->errstr : "unknown");
        return false;
    }
    if (!password_.empty()) {
        auto* reply = static_cast<redisReply*>(redisCommand(context_, "AUTH %s", password_.c_str()));
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            LOG_ERROR("Redis AUTH failed");
            freeReplyObject(reply);
            disconnect();
            return false;
        }
        freeReplyObject(reply);
    }
    connected_ = true;
    LOG_INFO("Redis connected to {}:{}", host_, port_);
    return true;
}

void RedisCache::disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
        connected_ = false;
    }
}

bool RedisCache::isConnected() const {
    return connected_;
}

bool RedisCache::set(const std::string& key, const std::string& value) {
    auto* reply = static_cast<redisReply*>(redisCommand(context_, "SET %s %s", key.c_str(), value.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS;
    freeReplyObject(reply);
    return ok;
}

bool RedisCache::setEx(const std::string& key, int ttl_seconds, const std::string& value) {
    auto* reply = static_cast<redisReply*>(redisCommand(context_, "SETEX %s %d %s",
                                                        key.c_str(), ttl_seconds, value.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS;
    freeReplyObject(reply);
    return ok;
}

std::string RedisCache::get(const std::string& key) {
    auto* reply = static_cast<redisReply*>(redisCommand(context_, "GET %s", key.c_str()));
    std::string result;
    if (reply && reply->type == REDIS_REPLY_STRING) {
        result = reply->str;
    }
    freeReplyObject(reply);
    return result;
}

bool RedisCache::del(const std::string& key) {
    auto* reply = static_cast<redisReply*>(redisCommand(context_, "DEL %s", key.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);
    return ok;
}

bool RedisCache::exists(const std::string& key) {
    auto* reply = static_cast<redisReply*>(redisCommand(context_, "EXISTS %s", key.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    freeReplyObject(reply);
    return ok;
}