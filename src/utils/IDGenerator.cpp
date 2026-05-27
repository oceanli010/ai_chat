#include "IDGenerator.h"

IDGenerator::IDGenerator()
    : epoch_(1609459200000ULL),
      machine_id_(generate_machine_id()),
      sequence_(0),
      last_timestamp_(0),
      base_(1000000000ULL),
      range_(9000000000ULL) {}

uint64_t IDGenerator::generate_machine_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, 0xFF);
    return dist(gen);
}

uint64_t IDGenerator::generate() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());

    if (now == last_timestamp_) {
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
        sequence_ = 0;
    }
    last_timestamp_ = now;

    uint64_t timestamp_part = (now - epoch_) & 0xFFFFFFFF;
    uint64_t id = (timestamp_part << 18) | (machine_id_ << 10) | sequence_;

    uint64_t result = base_ + (id % range_);
    return result;
}
