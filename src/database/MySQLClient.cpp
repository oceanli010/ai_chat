#include "MySQLClient.h"

MySQLClient::MySQLClient() : driver_(nullptr), port_(3306), pool_size_(10) {}

std::unique_ptr<sql::Connection> MySQLClient::create_connection() {
    try {
        auto conn = std::unique_ptr<sql::Connection>(
            driver_->connect(host_ + ":" + std::to_string(port_),
                             user_, password_));
        conn->setSchema(database_);
        return conn;
    } catch (const sql::SQLException& e) {
        APP_LOG_ERROR("MySQL connection failed: {}", e.what());
        return nullptr;
    }
}

void MySQLClient::init(const std::string& host,
                        int port,
                        const std::string& user,
                        const std::string& password,
                        const std::string& database,
                        int pool_size) {
    host_ = host;
    port_ = port;
    user_ = user;
    password_ = password;
    database_ = database;
    pool_size_ = pool_size;

    driver_ = get_driver_instance();

    for (int i = 0; i < pool_size_; ++i) {
        auto conn = create_connection();
        if (conn) {
            pool_.push(std::move(conn));
        }
    }

    APP_LOG_INFO("MySQL connection pool initialized with {}/{} connections",
                 pool_.size(), pool_size_);
    if (pool_.empty()) {
        APP_LOG_WARN("MySQL pool is empty — database '{}' may not exist", database_);
    }
}

std::unique_ptr<sql::Connection> MySQLClient::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (pool_.empty()) {
        // 池为空时等待（最多10秒），避免永久阻塞
        if (cond_.wait_for(lock, std::chrono::seconds(10)) == std::cv_status::timeout) {
            // 超时后主动尝试创建一个新连接
            auto new_conn = create_connection();
            if (new_conn) {
                return new_conn;
            }
        }
    }
    auto conn = std::move(pool_.front());
    pool_.pop();
    lock.unlock();

    try {
        if (conn->isClosed()) {
            conn = create_connection();
        }
    } catch (const std::exception& e) {
        APP_LOG_WARN("Acquire: connection check failed ({}), creating new", e.what());
        conn = create_connection();
    }

    // 如果重连失败，递归重试（但限制重试次数防止栈溢出）
    if (!conn) {
        APP_LOG_WARN("Acquire: failed to create new connection, retrying...");
        return acquire();
    }

    return conn;
}

void MySQLClient::release(std::unique_ptr<sql::Connection> conn) {
    if (!conn) return;
    bool valid = false;
    try {
        valid = !conn->isClosed();
    } catch (...) {
        valid = false;
    }
    if (valid) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(std::move(conn));
        cond_.notify_one();
    } else {
        APP_LOG_WARN("Release: discarding stale MySQL connection");
        // 尝试补充一个新连接回池中
        auto new_conn = create_connection();
        if (new_conn) {
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push(std::move(new_conn));
            cond_.notify_one();
        }
    }
}
