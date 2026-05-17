add_library(server_network STATIC
    src/tls_context.cpp
    src/protocol.cpp
    src/http_server.cpp
    src/ws_server.cpp
    src/session.cpp
)

target_include_directories(server_network PUBLIC include)
target_link_libraries(server_network PUBLIC
    server_infrastructure
    server_model
    server_business
    Boost::boost
    OpenSSL::SSL
    nlohmann_json::nlohmann_json
)