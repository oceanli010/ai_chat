#pragma once

#include "data/mysql_database.h"
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <cstddef>

class ConnectionPool {
public:
    ConnectionPool(const std::string& host, int port,
                   const std::string& user, const std::string& password,
                   const std::string& database, size_t min_conn, size_t max_conn);
    ~ConnectionPool();

    std::shared_ptr<MySQLDatabase> acquire();
    void release(std::shared_ptr<MySQLDatabase> conn);
    size_t available() const;
    size_t total() const;

private:
    void createConnections(size_t count);

    std::queue<std::shared_ptr<MySQLDatabase>> pool_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;

    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_;
    size_t min_connections_;
    size_t max_connections_;
    size_t total_connections_ = 0;
};