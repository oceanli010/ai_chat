include(${CMAKE_SOURCE_DIR}/cmake/CompilerOptions.cmake)

add_subdirectory(infrastructure)
add_subdirectory(model)
add_subdirectory(data)
add_subdirectory(business)
add_subdirectory(network)

find_library(MYSQL_CLIENT_LIB mysqlclient REQUIRED)

add_executable(ai_chat_server main.cpp)
target_include_directories(ai_chat_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ai_chat_server PRIVATE
    server_infrastructure
    server_model
    server_data
    server_business
    server_network
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    CURL::libcurl
    ${MYSQL_CLIENT_LIB}
    hiredis
)

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()