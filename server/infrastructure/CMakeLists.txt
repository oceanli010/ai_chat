add_library(server_infrastructure STATIC
    src/logger.cpp
    src/config.cpp
    src/thread_pool.cpp
    src/crypto_util.cpp
    src/token_manager.cpp
    src/email_sender.cpp
)

target_include_directories(server_infrastructure PUBLIC include)
target_link_libraries(server_infrastructure PUBLIC
    spdlog::spdlog
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    CURL::libcurl
)