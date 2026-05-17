#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

class Logger {
public:
    static Logger& instance();

    void init(const std::string& log_file_path, spdlog::level::level_enum level);
    spdlog::logger* get();

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::shared_ptr<spdlog::logger> logger_;
};

#define LOG_TRACE(...)    Logger::instance().get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    Logger::instance().get()->debug(__VA_ARGS__)
#define LOG_INFO(...)     Logger::instance().get()->info(__VA_ARGS__)
#define LOG_WARN(...)     Logger::instance().get()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    Logger::instance().get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::instance().get()->critical(__VA_ARGS__)

#define DECLARE_MODULE_LOGGER(module_name) \
    static std::shared_ptr<spdlog::logger> module_logger = \
        std::make_shared<spdlog::logger>(module_name, \
            Logger::instance().get()->sinks().begin(), \
            Logger::instance().get()->sinks().end()); \
    spdlog::register_logger(module_logger)
