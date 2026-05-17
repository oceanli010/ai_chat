#pragma once

#include <string>
#include <memory>

class TlsContext {
public:
    TlsContext(const std::string& cert_file, const std::string& key_file);
    ~TlsContext();

    bool init();
    void* nativeContext();

private:
    std::string cert_file_;
    std::string key_file_;
    void* ssl_ctx_ = nullptr;
};