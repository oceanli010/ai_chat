#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>

class Logger {
public:
    static void init(const std::string& log_file,
                     const std::string& level = "info",
                     size_t max_size = 10485760,
                     size_t max_files = 7) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, max_size, max_files);

        spdlog::sinks_init_list sink_list = {console_sink, file_sink};

        auto logger = std::make_shared<spdlog::logger>("ai_chat", sink_list.begin(), sink_list.end());
        logger->set_level(spdlog::level::from_str(level));
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
        logger->flush_on(spdlog::level::info);

        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
    }

    static std::shared_ptr<spdlog::logger> get() {
        return spdlog::get("ai_chat");
    }
};

#define APP_LOG_TRACE(...)    spdlog::get("ai_chat")->trace(__VA_ARGS__)
#define APP_LOG_DEBUG(...)    spdlog::get("ai_chat")->debug(__VA_ARGS__)
#define APP_LOG_INFO(...)     spdlog::get("ai_chat")->info(__VA_ARGS__)
#define APP_LOG_WARN(...)     spdlog::get("ai_chat")->warn(__VA_ARGS__)
#define APP_LOG_ERROR(...)    spdlog::get("ai_chat")->error(__VA_ARGS__)
#define APP_LOG_CRITICAL(...) spdlog::get("ai_chat")->critical(__VA_ARGS__)
