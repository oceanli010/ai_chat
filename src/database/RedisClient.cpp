#include "RedisClient.h"

RedisClient::RedisClient() : context_(nullptr), port_(6379), db_(0) {}

RedisClient::~RedisClient() {
    if (context_) {
        redisFree(context_);
    }
}

// init
// 功能：初始化 Redis 连接，包含 TCP 连接、密码认证和数据库选择
// 参数：host - 服务器地址；port - 端口号；password - 认证密码；db - 数据库编号
// 说明：如果密码不为空则执行 AUTH 命令；db > 0 则执行 SELECT 命令切换数据库
void RedisClient::init(const std::string& host,
                        int port,
                        const std::string& password,
                        int db) {
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

// set
// 功能：设置指定 key 的字符串值
// 参数：key - 键；value - 值
// 返回值：bool - 操作成功返回 true
bool RedisClient::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "SET %s %s",
                                             key.c_str(), value.c_str());
    bool ok = reply && reply->type == REDIS_REPLY_STATUS &&
              std::string(reply->str) == "OK";
    freeReplyObject(reply);
    return ok;
}

// setex
// 功能：设置指定 key 的字符串值并指定过期时间
// 参数：key - 键；seconds - 过期秒数；value - 值
// 返回值：bool - 操作成功返回 true
bool RedisClient::setex(const std::string& key, int seconds, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "SETEX %s %d %s",
                                             key.c_str(), seconds,
                                             value.c_str());
    bool ok = reply && reply->type == REDIS_REPLY_STATUS &&
              std::string(reply->str) == "OK";
    freeReplyObject(reply);
    return ok;
}

// get
// 功能：获取指定 key 的字符串值
// 参数：key - 键
// 返回值：std::string - key 对应的值，key 不存在返回空字符串
std::string RedisClient::get(const std::string& key) {
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

// del
// 功能：删除指定 key
// 参数：key - 待删除的键
// 返回值：bool - 操作成功返回 true
bool RedisClient::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "DEL %s", key.c_str());
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);
    return ok;
}

// del_batch
// 功能：批量删除多个 key（使用 Redis 事务 MULTI/EXEC 保证原子性）
// 参数：keys - 待删除的键列表
void RedisClient::del_batch(const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> lock(mutex_);
    redisCommand(context_, "MULTI");
    for (const auto& key : keys) {
        redisCommand(context_, "DEL %s", key.c_str());
    }
    redisReply* reply = (redisReply*)redisCommand(context_, "EXEC");
    freeReplyObject(reply);
}

// incr
// 功能：对指定 key 执行自增操作
// 参数：key - 键
// 返回值：long long - 自增后的值，操作失败返回 0
long long RedisClient::incr(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "INCR %s", key.c_str());
    long long val = 0;
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        val = reply->integer;
    }
    freeReplyObject(reply);
    return val;
}

// smembers
// 功能：获取集合所有成员
// 参数：key - 集合的键
// 返回值：std::vector<std::string> - 成员字符串列表
std::vector<std::string> RedisClient::smembers(const std::string& key) {
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

// sadd
// 功能：向集合中添加一个成员
// 参数：key - 集合的键；member - 要添加的成员
// 返回值：long long - 成功添加的数量
long long RedisClient::sadd(const std::string& key, const std::string& member) {
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

// srem
// 功能：从集合中移除一个成员
// 参数：key - 集合的键；member - 要移除的成员
// 返回值：long long - 成功移除的数量
long long RedisClient::srem(const std::string& key, const std::string& member) {
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

// scard
// 功能：获取集合的成员数量
// 参数：key - 集合的键
// 返回值：long long - 集合成员数
long long RedisClient::scard(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "SCARD %s", key.c_str());
    long long val = 0;
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        val = reply->integer;
    }
    freeReplyObject(reply);
    return val;
}

// sismember
// 功能：检查成员是否存在于集合中
// 参数：key - 集合的键；member - 要检查的成员
// 返回值：bool - 存在返回 true
bool RedisClient::sismember(const std::string& key, const std::string& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "SISMEMBER %s %s",
                                             key.c_str(), member.c_str());
    bool result = reply && reply->type == REDIS_REPLY_INTEGER &&
                  reply->integer == 1;
    freeReplyObject(reply);
    return result;
}

// exists
// 功能：检查 key 是否存在
// 参数：key - 键
// 返回值：bool - 存在返回 true
bool RedisClient::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "EXISTS %s", key.c_str());
    bool result = reply && reply->type == REDIS_REPLY_INTEGER &&
                  reply->integer == 1;
    freeReplyObject(reply);
    return result;
}

// expire
// 功能：设置 key 的过期时间
// 参数：key - 键；seconds - 过期秒数
// 返回值：bool - 设置成功返回 true
bool RedisClient::expire(const std::string& key, int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "EXPIRE %s %d",
                                             key.c_str(), seconds);
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER &&
              reply->integer == 1;
    freeReplyObject(reply);
    return ok;
}

// ttl
// 功能：获取 key 的剩余过期时间
// 参数：key - 键
// 返回值：long long - 剩余秒数，key 不存在或没有过期时间返回 -1
long long RedisClient::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "TTL %s", key.c_str());
    long long val = -1;
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        val = reply->integer;
    }
    freeReplyObject(reply);
    return val;
}
