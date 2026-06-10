#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"
#include <memory>
#include <vector>


int main() {
    // 异步日志线程池：队列 8192 条，1 个后台线程
    spdlog::init_thread_pool(8192, 1);

    std::vector<spdlog::sink_ptr> sinks;

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::debug);

    auto rotatingFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/app.log", 10 * 1024 * 1024, 5);
    rotatingFileSink->set_level(spdlog::level::debug);

    sinks.emplace_back(consoleSink);
    sinks.emplace_back(rotatingFileSink);

    auto logger = std::make_shared<spdlog::async_logger>(
        "app",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );

    logger->set_level(spdlog::level::debug);

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");

    // 设置为默认 logger
    spdlog::set_default_logger(logger);

    // info 及以上立即 flush
    spdlog::flush_on(spdlog::level::info);

    // 每 3 秒自动 flush 一次
    spdlog::flush_every(std::chrono::seconds(3));



    spdlog::info("application started");
    spdlog::debug("debug message: {}", 123);
    spdlog::warn("warning message");
    spdlog::error("error message");


    spdlog::shutdown();

    return 0;
}