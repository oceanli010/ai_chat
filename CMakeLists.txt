cmake_minimum_required(VERSION 3.20)
project(ai_chat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# spdlog: apt install libspdlog-dev or vcpkg install spdlog
find_package(spdlog REQUIRED)

# nlohmann_json: apt install nlohmann-json3-dev or vcpkg install nlohmann-json
find_package(nlohmann_json REQUIRED)

# hiredis: apt install libhiredis-dev or vcpkg install hiredis
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS hiredis REQUIRED IMPORTED_TARGET)

# mysqlcppconn: apt install libmysqlcppconn-dev or vcpkg install mysql-connector-cpp
if(NOT mysqlcppconn_FOUND)
    find_library(MARIADB_LIB mysqlcppconn REQUIRED)
    find_path(MARIADB_INCLUDE mysql_driver.h
        PATHS /usr/include/mysql-cppconn-8 /usr/include/mysql
    )
    add_library(mysqlcppconn::mysqlcppconn UNKNOWN IMPORTED)
    set_target_properties(mysqlcppconn::mysqlcppconn PROPERTIES
        IMPORTED_LOCATION "${MARIADB_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MARIADB_INCLUDE}"
    )
endif()

# Drogon: vcpkg install drogon[mysql,redis]
# Drogon[mysql] depends on libmariadb
find_package(unofficial-libmariadb REQUIRED)
find_package(Drogon REQUIRED)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(ai_chat_server ${SOURCES})

target_include_directories(ai_chat_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/config
    ${nlohmann_json_INCLUDE_DIRS}
)

target_link_libraries(ai_chat_server PRIVATE
    Drogon::Drogon
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    PkgConfig::HIREDIS
    mysqlcppconn::mysqlcppconn
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    dl
)

install(TARGETS ai_chat_server DESTINATION bin)
install(DIRECTORY views/ DESTINATION share/ai_chat/views)
install(DIRECTORY config/ DESTINATION share/ai_chat/config)