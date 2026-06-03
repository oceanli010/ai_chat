#pragma once

#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include "utils/Logger.h"

// RedisClient
// Redis 客户端封装类，提供单例模式和线程安全的 Redis 命令操作接口
class RedisClient {
public:
    // instance
    // 功能：获取 RedisClient 单例实例
    // 返回值：RedisClient& - 单例引用
    static RedisClient& instance() {
        static RedisClient inst;
        return inst;
    }

    // init
    // 功能：初始化 Redis 连接，包含密码认证和数据库选择
    // 参数：host - 服务器地址；port - 端口号；password - 认证密码（可选）；db - 数据库编号，默认 0
    void init(const std::string& host,
              int port,
              const std::string& password = "",
              int db = 0);

    // set
    // 功能：设置指定 key 的字符串值
    // 参数：key - 键；value - 值
    // 返回值：bool - 设置成功返回 true
    bool set(const std::string& key, const std::string& value);

    // setex
    // 功能：设置指定 key 的字符串值并指定过期时间
    // 参数：key - 键；seconds - 过期秒数；value - 值
    // 返回值：bool - 设置成功返回 true
    bool setex(const std::string& key, int seconds, const std::string& value);

    // get
    // 功能：获取指定 key 的字符串值
    // 参数：key - 键
    // 返回值：std::string - key 对应的值，不存在则返回空字符串
    std::string get(const std::string& key);

    // del
    // 功能：删除指定 key
    // 参数：key - 待删除的键
    // 返回值：bool - 删除成功返回 true
    bool del(const std::string& key);

    // del_batch
    // 功能：批量删除多个 key（使用 Redis 事务 MULTI/EXEC 保证原子性）
    // 参数：keys - 待删除的键列表
    void del_batch(const std::vector<std::string>& keys);

    // incr
    // 功能：对指定 key 执行自增操作
    // 参数：key - 键
    // 返回值：long long - 自增后的值
    long long incr(const std::string& key);

    // smembers
    // 功能：获取集合中所有成员
    // 参数：key - 集合的键
    // 返回值：std::vector<std::string> - 集合成员列表
    std::vector<std::string> smembers(const std::string& key);

    // sadd
    // 功能：向集合中添加一个成员
    // 参数：key - 集合的键；member - 要添加的成员
    // 返回值：long long - 成功添加的数量
    long long sadd(const std::string& key, const std::string& member);

    // srem
    // 功能：从集合中移除一个成员
    // 参数：key - 集合的键；member - 要移除的成员
    // 返回值：long long - 成功移除的数量
    long long srem(const std::string& key, const std::string& member);

    // scard
    // 功能：获取集合的成员数量
    // 参数：key - 集合的键
    // 返回值：long long - 集合成员数
    long long scard(const std::string& key);

    // sismember
    // 功能：检查成员是否存在于集合中
    // 参数：key - 集合的键；member - 要检查的成员
    // 返回值：bool - 存在返回 true
    bool sismember(const std::string& key, const std::string& member);

    // exists
    // 功能：检查 key 是否存在
    // 参数：key - 键
    // 返回值：bool - 存在返回 true
    bool exists(const std::string& key);

    // expire
    // 功能：设置 key 的过期时间
    // 参数：key - 键；seconds - 过期秒数
    // 返回值：bool - 设置成功返回 true
    bool expire(const std::string& key, int seconds);

    // ttl
    // 功能：获取 key 的剩余过期时间
    // 参数：key - 键
    // 返回值：long long - 剩余秒数，key 不存在返回 -1
    long long ttl(const std::string& key);

    ~RedisClient();

private:
    RedisClient();

    redisContext* context_;    // Redis 连接上下文
    std::string host_;         // Redis 服务器地址
    int port_;                 // Redis 端口号
    std::string password_;     // Redis 认证密码
    int db_;                   // 选中的数据库编号
    std::mutex mutex_;         // 线程安全互斥锁
};
