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

// MySQLClient
// MySQL 数据库连接池客户端，提供连接的获取、归还及作用域管理功能
class MySQLClient {
public:
    // instance
    // 功能：获取 MySQLClient 单例实例
    // 返回值：MySQLClient& - 单例引用
    static MySQLClient& instance() {
        static MySQLClient inst;
        return inst;
    }

    // init
    // 功能：初始化数据库连接池，创建指定数量的连接并加入池中
    // 参数：host - 数据库主机地址；port - 端口号；user - 用户名；password - 密码；database - 数据库名称；pool_size - 连接池大小，默认 10
    void init(const std::string& host,
              int port,
              const std::string& user,
              const std::string& password,
              const std::string& database,
              int pool_size = 10);

    // acquire
    // 功能：从连接池获取一个数据库连接，池为空时等待或创建新连接
    // 返回值：std::unique_ptr<sql::Connection> - 数据库连接智能指针
    std::unique_ptr<sql::Connection> acquire();

    // release
    // 功能：将数据库连接归还到连接池
    // 参数：conn - 待归还的连接
    void release(std::unique_ptr<sql::Connection> conn);

    // acquireScoped
    // 功能：获取作用域管理的数据库连接（RAII 方式），析构时自动归还
    // 返回值：ScopedConnection - 作用域连接对象
    static ScopedConnection acquireScoped();

    // get_driver
    // 功能：获取 MySQL 驱动指针
    // 返回值：sql::Driver* - MySQL 驱动指针
    sql::Driver* get_driver() { return driver_; }

private:
    MySQLClient();
    // create_connection
    // 功能：创建一条新的 MySQL 数据库连接
    // 返回值：std::unique_ptr<sql::Connection> - 新连接，创建失败返回 nullptr
    std::unique_ptr<sql::Connection> create_connection();

    sql::Driver* driver_;                                 // MySQL 驱动实例
    std::string host_;                                    // 数据库主机地址
    int port_;                                            // 数据库端口号
    std::string user_;                                    // 数据库用户名
    std::string password_;                                // 数据库密码
    std::string database_;                                // 数据库名称
    int pool_size_;                                       // 连接池大小

    std::mutex mutex_;                                    // 线程安全互斥锁
    std::condition_variable cond_;                        // 条件变量，用于连接可用时唤醒等待线程
    std::queue<std::unique_ptr<sql::Connection>> pool_;   // 连接池队列
};

// ScopedConnection
// RAII 作用域连接包装类，析构时自动将连接归还到 MySQLClient 连接池
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

    // valid
    // 功能：检查连接是否有效且未关闭
    // 返回值：bool - 连接有效返回 true
    bool valid() const {
        try {
            return conn_ != nullptr && !conn_->isClosed();
        } catch (...) {
            return false;
        }
    }

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection(ScopedConnection&&) noexcept = default;
    ScopedConnection& operator=(ScopedConnection&&) noexcept = default;

private:
    std::unique_ptr<sql::Connection> conn_;  // 持有的数据库连接
};

inline ScopedConnection MySQLClient::acquireScoped() {
    return ScopedConnection(instance().acquire());
}
