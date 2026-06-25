#include "realtime/FFmpegRealtimeStreamTranscodeEngine.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

struct Options {
    media::RealtimeCoreConfig config;
    int durationSeconds = 10;
    int loopCount = 1;
    bool acceptEof = false;
    bool verbose = false;
    bool help = false;
};

std::string requireValue(int argc, char* argv[], int& index, const std::string& name)
{
    if (index + 1 >= argc) {
        throw std::runtime_error("missing value for " + name);
    }
    return argv[++index];
}

int parseInt(const std::string& value, const std::string& name, int minValue)
{
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < minValue) {
        throw std::runtime_error("invalid value for " + name + ": " + value);
    }
    return static_cast<int>(parsed);
}

void printUsage(const char* exe)
{
    std::cout
        << "Usage:\n"
        << "  " << exe << " --input <url-or-path> [options]\n\n"
        << "Options:\n"
        << "  -i, --input <url-or-path>      RTSP/RTP/UDP URL or local media path\n"
        << "      --duration <seconds>       Read duration before requestStop, default 10\n"
        << "      --loop <count>             Repeat start/stop cycles, default 1\n"
        << "      --open-timeout-ms <value>  Open/find-stream timeout, default 5000\n"
        << "      --read-timeout-ms <value>  av_read_frame timeout, default 5000\n"
        << "      --input-format <name>      Optional FFmpeg input format hint\n"
        << "      --accept-eof               Treat EOF as success for local-file probing\n"
        << "      --no-low-latency           Disable low-latency input options\n"
        << "      --verbose                  Enable debug logs\n"
        << "  -h, --help                     Show this help\n\n"
        << "This is an internal P1 tool. It validates realtime input -> decode -> encode counters.\n";
}

Options parseOptions(int argc, char* argv[])
{
    Options options;
    options.config.rtpOutput.host = "127.0.0.1";
    options.config.rtpOutput.rtpPort = 5004;
    options.config.rtpOutput.packetSize = 1200;
    options.config.openTimeoutMs = 5000;
    options.config.readTimeoutMs = 5000;
    options.config.analyzeDurationUs = 500000;
    options.config.probeSizeBytes = 512 * 1024;
    options.config.lowLatency = true;
    options.config.videoBitrateKbps = 2000;
    options.config.maxBFrames = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            options.help = true;
            return options;
        }
        if (arg == "-i" || arg == "--input") {
            options.config.inputUrl = requireValue(argc, argv, i, arg);
            continue;
        }
        if (arg == "--duration") {
            options.durationSeconds = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--loop") {
            options.loopCount = parseInt(requireValue(argc, argv, i, arg), arg, 1);
            continue;
        }
        if (arg == "--open-timeout-ms") {
            options.config.openTimeoutMs = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--read-timeout-ms") {
            options.config.readTimeoutMs = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--input-format") {
            options.config.inputFormatHint = requireValue(argc, argv, i, arg);
            continue;
        }
        if (arg == "--accept-eof") {
            options.acceptEof = true;
            continue;
        }
        if (arg == "--no-low-latency") {
            options.config.lowLatency = false;
            continue;
        }
        if (arg == "--verbose") {
            options.verbose = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }
        if (options.config.inputUrl.empty()) {
            options.config.inputUrl = arg;
            continue;
        }
        throw std::runtime_error("too many positional arguments: " + arg);
    }

    if (!options.help && options.config.inputUrl.empty()) {
        throw std::runtime_error("missing required option: --input");
    }
    return options;
}

bool isAcceptedEof(const media::ErrorInfo& error, bool acceptEof)
{
    return acceptEof && error.code == media::ErrorCode::IoFailure &&
           error.message.find("end of stream") != std::string::npos;
}

bool validateCounters(const media::RealtimeCoreStats& stats)
{
    if (stats.inputVideoPacketCount <= 0) {
        spdlog::error("probe failed: no video packets were read");
        return false;
    }
    if (stats.decodedVideoFrameCount <= 0) {
        spdlog::error("probe failed: no video frames were decoded");
        return false;
    }
    if (stats.encodedVideoPacketCount <= 0) {
        spdlog::error("probe failed: no video packets were encoded");
        return false;
    }
    return true;
}

bool runProbeOnce(const Options& options, int index)
{
    spdlog::info("P1 realtime probe iteration {}/{}", index, options.loopCount);

    media::FFmpegRealtimeStreamTranscodeEngine engine;
    engine.setProgressCallback([](const media::ProgressInfo& info) {
        spdlog::info("progress: stage={}, frame={}, timeMs={}",
                     info.raw,
                     info.frame,
                     info.outTimeMs);
    });

    const media::Status initStatus = engine.initialize(options.config);
    if (!initStatus) {
        spdlog::error("initialize failed: {}", initStatus.error().describe());
        return false;
    }

    const media::Status startStatus = engine.start();
    if (!startStatus) {
        spdlog::error("start failed: {}", startStatus.error().describe());
        return false;
    }

    if (options.durationSeconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(options.durationSeconds));
    }

    engine.requestStop();
    const media::Status waitStatus = engine.wait();
    const media::RealtimeCoreStats stats = engine.stats();

    spdlog::info("stats: inputPackets={}, inputVideoPackets={}, decodedFrames={}, encodedPackets={}, rtpPackets={}",
                 stats.inputPacketCount,
                 stats.inputVideoPacketCount,
                 stats.decodedVideoFrameCount,
                 stats.encodedVideoPacketCount,
                 stats.writtenRtpPacketCount);

    if (!waitStatus) {
        if (isAcceptedEof(waitStatus.error(), options.acceptEof)) {
            spdlog::warn("EOF accepted: {}", waitStatus.error().describe());
            return validateCounters(stats);
        }
        spdlog::error("wait failed: {}", waitStatus.error().describe());
        return false;
    }

    return validateCounters(stats);
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        auto logger = spdlog::stdout_color_mt("p1_realtime_probe");
        spdlog::set_default_logger(logger);

        const Options options = parseOptions(argc, argv);
        if (options.help) {
            printUsage(argv[0]);
            return 0;
        }

        spdlog::set_level(options.verbose ? spdlog::level::debug : spdlog::level::info);
        spdlog::info("input={}, duration={}s, loop={}, lowLatency={}",
                     options.config.inputUrl,
                     options.durationSeconds,
                     options.loopCount,
                     options.config.lowLatency ? "true" : "false");

        bool ok = true;
        for (int i = 1; i <= options.loopCount; ++i) {
            ok = runProbeOnce(options, i) && ok;
        }

        if (!ok) {
            spdlog::error("P1 realtime probe failed");
            return 1;
        }

        spdlog::info("P1 realtime probe succeeded");
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "p1 realtime probe error: " << e.what() << '\n';
        return 1;
    }
}
