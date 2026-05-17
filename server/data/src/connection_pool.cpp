#include "data/connection_pool.h"
#include "infrastructure/logger.h"

ConnectionPool::ConnectionPool(const std::string& host, int port,
                               const std::string& user, const std::string& password,
                               const std::string& database, size_t min_conn, size_t max_conn)
    : host_(host), port_(port), user_(user), password_(password),
      database_(database), min_connections_(min_conn), max_connections_(max_conn) {
    createConnections(min_connections_);
}

ConnectionPool::~ConnectionPool() {
    while (!pool_.empty()) {
        pool_.pop();
    }
    total_connections_ = 0;
}

void ConnectionPool::createConnections(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        auto conn = std::make_shared<MySQLDatabase>(host_, port_, user_, password_, database_);
        if (conn->connect()) {
            pool_.push(conn);
            ++total_connections_;
        } else {
            LOG_ERROR("Failed to create MySQL connection in pool");
        }
    }
}

std::shared_ptr<MySQLDatabase> ConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (pool_.empty() && total_connections_ < max_connections_) {
        createConnections(1);
    }
    condition_.wait(lock, [this]() { return !pool_.empty(); });
    auto conn = pool_.front();
    pool_.pop();
    return conn;
}

void ConnectionPool::release(std::shared_ptr<MySQLDatabase> conn) {
    if (!conn) return;
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push(conn);
    condition_.notify_one();
}

size_t ConnectionPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

size_t ConnectionPool::total() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_connections_;
}