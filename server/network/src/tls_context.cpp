#include "network/tls_context.h"
#include "infrastructure/logger.h"

TlsContext::TlsContext(const std::string& cert_file, const std::string& key_file)
    : cert_file_(cert_file), key_file_(key_file) {}

TlsContext::~TlsContext() = default;

bool TlsContext::init() {
    LOG_INFO("TlsContext::init: cert={}, key={}", cert_file_, key_file_);
    return true;
}

void* TlsContext::nativeContext() {
    return ssl_ctx_;
}