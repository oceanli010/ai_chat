#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <mutex>

// IDGenerator
// 功能：分布式唯一 ID 生成器，基于雪花算法（Snowflake）实现
// 说明：使用 64 位 ID，结构为 timestamp(32bit) | machine_id(8bit) | sequence(10bit)
//       最终结果映射到 [base, base+range) 范围内
class IDGenerator {
public:
    // instance
    // 功能：获取 IDGenerator 单例
    // 参数：无
    // 返回值：IDGenerator& - 全局唯一的 ID 生成器实例
    static IDGenerator& instance() {
        static IDGenerator inst;
        return inst;
    }

    // generate
    // 功能：生成一个全局唯一的 64 位 ID
    // 参数：无
    // 返回值：uint64_t - 生成的唯一 ID
    // 说明：线程安全，支持同一毫秒内生成多个 ID（通过序列号递增）
    uint64_t generate();

private:
    IDGenerator();
    uint64_t generate_machine_id();

    uint64_t epoch_;          // 起始时间戳（毫秒），用于减少 ID 长度
    uint64_t machine_id_;     // 机器标识（0~255）
    uint64_t sequence_;       // 当前毫秒内的序列号（0~1023）
    uint64_t last_timestamp_; // 上次生成 ID 的时间戳（毫秒）
    uint64_t base_;           // ID 基数下限
    uint64_t range_;          // ID 范围，最终 ID = base_ + (id % range_)
    std::mutex mutex_;        // 生成 ID 时的互斥锁
};
