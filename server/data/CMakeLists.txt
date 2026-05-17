add_library(server_data STATIC
    src/database.cpp
    src/mysql_database.cpp
    src/redis_cache.cpp
    src/connection_pool.cpp
    src/user_repository.cpp
    src/chat_repository.cpp
)

target_include_directories(server_data PUBLIC include)
target_link_libraries(server_data PUBLIC
    server_infrastructure
    server_model
    OpenSSL::SSL
    hiredis
)