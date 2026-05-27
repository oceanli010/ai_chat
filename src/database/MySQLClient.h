#pragma once

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "utils/Logger.h"

class ScopedConnection;

class MySQLClient {
public:
    static MySQLClient& instance() {
        static MySQLClient inst;
        return inst;
    }

    void init(const std::string& host,
              int port,
              const std::string& user,
              const std::string& password,
              const std::string& database,
              int pool_size = 10) {
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

    std::unique_ptr<sql::Connection> acquire() {
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

    void release(std::unique_ptr<sql::Connection> conn) {
        if (conn && !conn->isClosed()) {
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push(std::move(conn));
            cond_.notify_one();
        }
    }

    static ScopedConnection acquireScoped();

    sql::Driver* get_driver() { return driver_; }

private:
    MySQLClient() : driver_(nullptr), port_(3306), pool_size_(10) {}

    std::unique_ptr<sql::Connection> create_connection() {
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

    sql::Driver* driver_;
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_;
    int pool_size_;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::queue<std::unique_ptr<sql::Connection>> pool_;
};

class ScopedConnection {
public:
    explicit ScopedConnection(std::unique_ptr<sql::Connection> conn) : conn_(std::move(conn)) {}

    ~ScopedConnection() {
        if (conn_) {
            MySQLClient::instance().release(std::move(conn_));
        }
    }

    sql::Connection* operator->() const { return conn_.get(); }
    sql::Connection& operator*() const { return *conn_; }
    sql::Connection* get() const { return conn_.get(); }

    bool valid() const { return conn_ != nullptr && !conn_->isClosed(); }

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection(ScopedConnection&&) noexcept = default;
    ScopedConnection& operator=(ScopedConnection&&) noexcept = default;

private:
    std::unique_ptr<sql::Connection> conn_;
};

inline ScopedConnection MySQLClient::acquireScoped() {
    return ScopedConnection(instance().acquire());
}
