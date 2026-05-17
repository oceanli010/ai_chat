#include "infrastructure/logger.h"
#include <filesystem>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& log_file_path, spdlog::level::level_enum level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(level);

    std::string actual_path = log_file_path;
    if (!log_file_path.empty() && !std::filesystem::path(log_file_path).has_extension()) {
        std::filesystem::create_directories(log_file_path);
        actual_path = log_file_path + "/server.log";
    }

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        actual_path, 5 * 1024 * 1024, 3);
    file_sink->set_level(level);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    logger_ = std::make_shared<spdlog::logger>("ai_chat", sinks.begin(), sinks.end());
    logger_->set_level(level);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
    logger_->flush_on(spdlog::level::err);
    spdlog::register_logger(logger_);
}

spdlog::logger* Logger::get() {
    return logger_.get();
}