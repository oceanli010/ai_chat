#pragma once

#include "data/database.h"
#include <mysql/mysql.h>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

class MySQLPreparedStatement {
public:
    explicit MySQLPreparedStatement(MYSQL_STMT* stmt);
    ~MySQLPreparedStatement();
    
    void setString(int index, const std::string& value);
    void setInt(int index, int value);
    void setInt64(int index, int64_t value);
    
    bool executeUpdate();
    std::shared_ptr<class MySQLResult> executeQuery();
    
private:
    struct BindingData {
        MYSQL_BIND bind;
        std::string string_value;
        int int_value;
        int64_t int64_value;
    };
    
    MYSQL_STMT* stmt_;
    std::vector<BindingData> bindings_;
};

class MySQLResult {
public:
    MySQLResult(MYSQL_RES* res, MYSQL_STMT* stmt, MYSQL_BIND* bind, std::vector<char*>& data, std::vector<unsigned long>& lengths, std::unique_ptr<bool[]>& null_indicators);
    ~MySQLResult();
    
    bool next();
    std::string getString(const std::string& column);
    int getInt(const std::string& column);
    int64_t getInt64(const std::string& column);
    
private:
    MYSQL_RES* res_;
    MYSQL_STMT* stmt_;
    MYSQL_BIND* result_bind_;
    std::vector<char*> row_data_;
    std::vector<unsigned long> lengths_;
    std::unique_ptr<bool[]> null_indicators_;
    MYSQL_FIELD* fields_;
    unsigned int num_fields_;
};

class MySQLDatabase : public Database {
public:
    explicit MySQLDatabase(const std::string& host, int port,
                           const std::string& user, const std::string& password,
                           const std::string& database);
    ~MySQLDatabase() override;

    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;

    MYSQL* nativeHandle() { return mysql_; }
    
    std::shared_ptr<MySQLPreparedStatement> prepareStatement(const std::string& query);

private:
    MYSQL* mysql_ = nullptr;
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_;
    bool connected_ = false;
};
