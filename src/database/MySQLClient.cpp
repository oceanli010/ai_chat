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

    APP_LOG_INFO("MySQL connection pool initialized with {} connections", pool_size_);
}

std::unique_ptr<sql::Connection> MySQLClient::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (pool_.empty()) {
        cond_.wait(lock);
    }
    auto conn = std::move(pool_.front());
    pool_.pop();

    if (conn->isClosed()) {
        conn = create_connection();
        if (!conn) {
            while (pool_.empty()) {
                cond_.wait(lock);
            }
            conn = std::move(pool_.front());
            pool_.pop();
        }
    }

    return conn;
}

void MySQLClient::release(std::unique_ptr<sql::Connection> conn) {
    if (conn && !conn->isClosed()) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(std::move(conn));
        cond_.notify_one();
    }
}
