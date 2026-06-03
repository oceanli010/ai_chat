#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>

// Logger
// 功能：全局日志管理器，基于 spdlog 库提供控制台和文件双输出
// 说明：初始化后通过宏（APP_LOG_xxx）在任意位置记录日志
class Logger {
public:
    // init
    // 功能：初始化日志系统，创建控制台和滚动文件两个 sink
    // 参数：log_file - 日志文件路径（字符串）
    //       level - 日志级别字符串，如 "info", "debug", "warn", "error"（默认 "info"）
    //       max_size - 单个日志文件最大字节数（默认 10MB）
    //       max_files - 保留的滚动文件数量（默认 7）
    // 返回值：无
    // 说明：日志格式为 [时间] [级别] [源文件:行号] 消息
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

    // get
    // 功能：获取已注册的 ai_chat 日志器实例
    // 参数：无
    // 返回值：shared_ptr<spdlog::logger> - 日志器智能指针
    static std::shared_ptr<spdlog::logger> get() {
        return spdlog::get("ai_chat");
    }
};

// 便捷日志宏，分别对应 trace / debug / info / warn / error / critical 级别
#define APP_LOG_TRACE(...)    spdlog::get("ai_chat")->trace(__VA_ARGS__)
#define APP_LOG_DEBUG(...)    spdlog::get("ai_chat")->debug(__VA_ARGS__)
#define APP_LOG_INFO(...)     spdlog::get("ai_chat")->info(__VA_ARGS__)
#define APP_LOG_WARN(...)     spdlog::get("ai_chat")->warn(__VA_ARGS__)
#define APP_LOG_ERROR(...)    spdlog::get("ai_chat")->error(__VA_ARGS__)
#define APP_LOG_CRITICAL(...) spdlog::get("ai_chat")->critical(__VA_ARGS__)
