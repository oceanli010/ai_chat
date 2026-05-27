#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <mutex>

class IDGenerator {
public:
    static IDGenerator& instance() {
        static IDGenerator inst;
        return inst;
    }

    uint64_t generate();

private:
    IDGenerator();
    uint64_t generate_machine_id();

    uint64_t epoch_;
    uint64_t machine_id_;
    uint64_t sequence_;
    uint64_t last_timestamp_;
    uint64_t base_;
    uint64_t range_;
    std::mutex mutex_;
};
