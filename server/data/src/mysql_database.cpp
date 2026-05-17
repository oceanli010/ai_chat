#include "data/mysql_database.h"
#include "infrastructure/logger.h"
#include <vector>

MySQLPreparedStatement::MySQLPreparedStatement(MYSQL_STMT* stmt) : stmt_(stmt) {
    bindings_.reserve(10);
}

MySQLPreparedStatement::~MySQLPreparedStatement() {
    if (stmt_) {
        mysql_stmt_close(stmt_);
    }
}

void MySQLPreparedStatement::setString(int index, const std::string& value) {
    bindings_.resize(std::max(bindings_.size(), (size_t)index));
    auto& data = bindings_[index - 1];
    data.string_value = value;
    data.bind.buffer_type = MYSQL_TYPE_STRING;
    data.bind.buffer = const_cast<char*>(data.string_value.c_str());
    data.bind.buffer_length = data.string_value.length();
    data.bind.length = &data.bind.buffer_length;
}

void MySQLPreparedStatement::setInt(int index, int value) {
    bindings_.resize(std::max(bindings_.size(), (size_t)index));
    auto& data = bindings_[index - 1];
    data.int_value = value;
    data.bind.buffer_type = MYSQL_TYPE_LONG;
    data.bind.buffer = &data.int_value;
}

void MySQLPreparedStatement::setInt64(int index, int64_t value) {
    bindings_.resize(std::max(bindings_.size(), (size_t)index));
    auto& data = bindings_[index - 1];
    data.int64_value = value;
    data.bind.buffer_type = MYSQL_TYPE_LONGLONG;
    data.bind.buffer = &data.int64_value;
}

bool MySQLPreparedStatement::executeUpdate() {
    if (bindings_.empty()) {
        return mysql_stmt_execute(stmt_) == 0 && mysql_stmt_affected_rows(stmt_) > 0;
    }
    
    std::vector<MYSQL_BIND> bind_params(bindings_.size());
    for (size_t i = 0; i < bindings_.size(); ++i) {
        bind_params[i] = bindings_[i].bind;
    }
    
    if (mysql_stmt_bind_param(stmt_, bind_params.data()) != 0) {
        LOG_ERROR("Failed to bind parameters: {}", mysql_stmt_error(stmt_));
        return false;
    }
    
    if (mysql_stmt_execute(stmt_) != 0) {
        LOG_ERROR("Failed to execute statement: {}", mysql_stmt_error(stmt_));
        return false;
    }
    
    return mysql_stmt_affected_rows(stmt_) > 0;
}

std::shared_ptr<MySQLResult> MySQLPreparedStatement::executeQuery() {
    if (!bindings_.empty()) {
        std::vector<MYSQL_BIND> bind_params(bindings_.size());
        for (size_t i = 0; i < bindings_.size(); ++i) {
            bind_params[i] = bindings_[i].bind;
        }
        
        if (mysql_stmt_bind_param(stmt_, bind_params.data()) != 0) {
            LOG_ERROR("Failed to bind parameters: {}", mysql_stmt_error(stmt_));
            return nullptr;
        }
    }
    
    if (mysql_stmt_execute(stmt_) != 0) {
        LOG_ERROR("Failed to execute query: {}", mysql_stmt_error(stmt_));
        return nullptr;
    }
    
    MYSQL_RES* res = mysql_stmt_result_metadata(stmt_);
    if (!res) {
        LOG_ERROR("Failed to get result metadata: {}", mysql_stmt_error(stmt_));
        return nullptr;
    }
    
    MYSQL_BIND* result_bind = new MYSQL_BIND[mysql_num_fields(res)];
    memset(result_bind, 0, sizeof(MYSQL_BIND) * mysql_num_fields(res));
    
    std::vector<char*> row_data;
    std::vector<unsigned long> lengths;
    auto null_indicators = std::make_unique<bool[]>(mysql_num_fields(res));
    row_data.resize(mysql_num_fields(res));
    lengths.resize(mysql_num_fields(res));
    
    for (unsigned int i = 0; i < mysql_num_fields(res); ++i) {
        unsigned long buffer_size = 65536;
        row_data[i] = new char[buffer_size];
        memset(row_data[i], 0, buffer_size);
        result_bind[i].buffer_type = MYSQL_TYPE_STRING;
        result_bind[i].buffer = row_data[i];
        result_bind[i].buffer_length = buffer_size;
        result_bind[i].length = &lengths[i];
        result_bind[i].is_null = &null_indicators[i];
    }
    
    if (mysql_stmt_bind_result(stmt_, result_bind) != 0) {
        LOG_ERROR("Failed to bind result: {}", mysql_stmt_error(stmt_));
        for (auto p : row_data) delete[] p;
        delete[] result_bind;
        mysql_free_result(res);
        return nullptr;
    }
    
    if (mysql_stmt_store_result(stmt_) != 0) {
        LOG_ERROR("Failed to store result: {}", mysql_stmt_error(stmt_));
        for (auto p : row_data) delete[] p;
        delete[] result_bind;
        mysql_free_result(res);
        return nullptr;
    }
    
    auto result = std::make_shared<MySQLResult>(res, stmt_, result_bind, row_data, lengths, null_indicators);
    return result;
}

MySQLResult::MySQLResult(MYSQL_RES* res, MYSQL_STMT* stmt, MYSQL_BIND* bind, std::vector<char*>& data, std::vector<unsigned long>& lengths, std::unique_ptr<bool[]>& null_indicators)
    : res_(res), stmt_(stmt), result_bind_(bind), row_data_(data), lengths_(std::move(lengths)), null_indicators_(std::move(null_indicators)) {
    fields_ = mysql_fetch_fields(res_);
    num_fields_ = mysql_num_fields(res_);
}

MySQLResult::~MySQLResult() {
    if (stmt_) {
        mysql_stmt_free_result(stmt_);
    }
    if (res_) {
        mysql_free_result(res_);
    }
    if (result_bind_) {
        delete[] result_bind_;
    }
    for (auto p : row_data_) {
        delete[] p;
    }
}

bool MySQLResult::next() {
    return mysql_stmt_fetch(stmt_) == 0;
}

std::string MySQLResult::getString(const std::string& column) {
    for (unsigned int i = 0; i < num_fields_; ++i) {
        if (std::string(fields_[i].name) == column) {
            if (row_data_[i]) {
                return std::string(row_data_[i], *result_bind_[i].length);
            }
            return "";
        }
    }
    return "";
}

int MySQLResult::getInt(const std::string& column) {
    for (unsigned int i = 0; i < num_fields_; ++i) {
        if (std::string(fields_[i].name) == column) {
            if (row_data_[i] && *result_bind_[i].length > 0) {
                try {
                    return std::stoi(row_data_[i]);
                } catch (...) {
                    return 0;
                }
            }
            return 0;
        }
    }
    return 0;
}

int64_t MySQLResult::getInt64(const std::string& column) {
    for (unsigned int i = 0; i < num_fields_; ++i) {
        if (std::string(fields_[i].name) == column) {
            if (row_data_[i] && *result_bind_[i].length > 0) {
                try {
                    return std::stoll(row_data_[i]);
                } catch (...) {
                    return 0;
                }
            }
            return 0;
        }
    }
    return 0;
}

MySQLDatabase::MySQLDatabase(const std::string& host, int port,
                             const std::string& user, const std::string& password,
                             const std::string& database)
    : host_(host), port_(port), user_(user), password_(password), database_(database) {
    mysql_ = mysql_init(nullptr);
}

MySQLDatabase::~MySQLDatabase() {
    disconnect();
}

bool MySQLDatabase::connect() {
    if (connected_) return true;
    if (!mysql_real_connect(mysql_, host_.c_str(), user_.c_str(), password_.c_str(),
                            database_.c_str(), port_, nullptr, 0)) {
        LOG_ERROR("MySQL connection failed: {}", mysql_error(mysql_));
        return false;
    }
    connected_ = true;
    LOG_INFO("MySQL connected to {}:{}", host_, port_);
    return true;
}

void MySQLDatabase::disconnect() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
        connected_ = false;
    }
}

bool MySQLDatabase::isConnected() const {
    return connected_;
}

std::shared_ptr<MySQLPreparedStatement> MySQLDatabase::prepareStatement(const std::string& query) {
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (!stmt) {
        LOG_ERROR("Failed to initialize prepared statement");
        return nullptr;
    }
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
        LOG_ERROR("Failed to prepare statement: {}", mysql_error(mysql_));
        mysql_stmt_close(stmt);
        return nullptr;
    }
    return std::make_shared<MySQLPreparedStatement>(stmt);
}
