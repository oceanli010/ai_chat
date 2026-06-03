#include "IDGenerator.h"

// IDGenerator（构造函数）
// 功能：初始化 ID 生成器，生成随机机器标识，设置 ID 范围
// 参数：无
// 说明：epoch 起点为 2021-01-01 00:00:00 UTC
IDGenerator::IDGenerator()
    : epoch_(1609459200000ULL),
      machine_id_(generate_machine_id()),
      sequence_(0),
      last_timestamp_(0),
      base_(1000000000ULL),
      range_(9000000000ULL) {}

// generate_machine_id
// 功能：生成 0~255 范围内的随机机器标识
// 参数：无
// 返回值：uint64_t - 机器标识值（0~255）
// 说明：使用 std::random_device 作为随机数种子
uint64_t IDGenerator::generate_machine_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, 0xFF);
    return dist(gen);
}

// generate
// 功能：生成一个全局唯一的 64 位 ID
// 参数：无
// 返回值：uint64_t - 基于雪花算法生成的唯一 ID
// 说明：线程安全；同一毫秒内序列号自增，序列号溢出时等待下一毫秒
uint64_t IDGenerator::generate() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());

    if (now == last_timestamp_) {
        // 同一毫秒内，序列号递增；溢出时等待下一毫秒
        sequence_ = (sequence_ + 1) & 0x3FF;
        if (sequence_ == 0) {
            while (now <= last_timestamp_) {
                now = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
            }
        }
    } else {
        // 新的一毫秒，序列号重置
        sequence_ = 0;
    }
    last_timestamp_ = now;

    // 拼接 ID：时间戳（相对 epoch）左移 18 位 | 机器标识左移 10 位 | 序列号
    uint64_t timestamp_part = (now - epoch_) & 0xFFFFFFFF;
    uint64_t id = (timestamp_part << 18) | (machine_id_ << 10) | sequence_;

    // 映射到 [base_, base_ + range_) 范围内
    uint64_t result = base_ + (id % range_);
    return result;
}
