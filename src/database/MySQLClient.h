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
              int pool_size = 10);

    std::unique_ptr<sql::Connection> acquire();

    void release(std::unique_ptr<sql::Connection> conn);

    static ScopedConnection acquireScoped();

    sql::Driver* get_driver() { return driver_; }

private:
    MySQLClient();
    std::unique_ptr<sql::Connection> create_connection();

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
