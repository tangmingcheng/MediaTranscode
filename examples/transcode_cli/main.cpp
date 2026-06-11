#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"
#include <memory>
#include <vector>
#include "media_transcode/FFmpegTranscoder.h"
#include <iostream>


int main(int argc, char* argv[]) {
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



    media::TranscodeConfig config;
    config.inputUrl = "video_ornament_mingwang_v1.mp4";
    config.outputUrl = "output.mp4";
    config.width = 1280;
    config.height = 720;
    config.fps = 25;
    config.videoCodec = media::VideoCodec::H264_LIBX264;
    config.audioMode = media::AudioMode::EncodeSelected;
    config.audioBitrateKbps = 128;
    config.videoBitrateKbps = 3000;

    media::FFmpegTranscoder transcoder;
    transcoder.setProgressCallback([](const media::ProgressInfo& info) {
        spdlog::info("Progress: frame={}, outTimeMs={}, speed={}x", info.frame, info.outTimeMs, info.speed);
        });

    if (!transcoder.initialize(config)) {
        spdlog::error("Transcoder initialize failed: {}", transcoder.lastError());
        return 1;
    }

    if (!transcoder.start()) {
        spdlog::error("Transcoder start failed: {}", transcoder.lastError());
        return 1;
    }

    transcoder.wait();

    if (!transcoder.lastError().empty()) {
        spdlog::error("Transcode failed: {}", transcoder.lastError());
        return 1;
    }

    spdlog::info("Transcode done");
   

    spdlog::shutdown();

    return 0;
}