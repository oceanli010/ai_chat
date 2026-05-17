add_library(server_model STATIC
    src/user.cpp
    src/chat_message.cpp
)

target_include_directories(server_model PUBLIC include)
target_link_libraries(server_model PUBLIC nlohmann_json::nlohmann_json)