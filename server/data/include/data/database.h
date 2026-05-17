#pragma once

#include <string>
#include <vector>
#include <memory>

class Database {
public:
    virtual ~Database() = default;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
};