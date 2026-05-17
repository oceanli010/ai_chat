add_executable(test_all
    test_crypto_util.cpp
    test_connection_pool.cpp
)
find_library(MYSQL_CLIENT_LIB mysqlclient REQUIRED)
target_link_libraries(test_all PRIVATE
    server_infrastructure
    server_data
    GTest::gtest
    GTest::gtest_main
    ${MYSQL_CLIENT_LIB}
    hiredis
)
add_test(NAME test_all COMMAND test_all)