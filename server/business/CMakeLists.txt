add_library(server_business STATIC
    src/service_locator.cpp
    src/auth_service.cpp
    src/user_service.cpp
    src/chat_service.cpp
    src/ai_service.cpp
    src/admin_service.cpp
)

target_include_directories(server_business PUBLIC include)
target_link_libraries(server_business PUBLIC
    server_infrastructure
    server_model
    server_data
    CURL::libcurl
    nlohmann_json::nlohmann_json
)