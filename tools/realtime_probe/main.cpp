#include "realtime/FFmpegRealtimeStreamTranscodeEngine.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <cctype>
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

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

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

media::VideoCodec parseVideoCodec(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "h264" || normalized == "avc") return media::VideoCodec::H264;
    if (normalized == "h265" || normalized == "hevc") return media::VideoCodec::H265;
    if (normalized == "mpeg4" || normalized == "mp4v") return media::VideoCodec::MPEG4;
    if (normalized == "vp8") return media::VideoCodec::VP8;
    if (normalized == "vp9") return media::VideoCodec::VP9;
    if (normalized == "av1") return media::VideoCodec::AV1;
    throw std::runtime_error("unsupported video codec: " + value);
}

media::VideoRateControlMode parseRateControl(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "auto") return media::VideoRateControlMode::Auto;
    if (normalized == "cbr") return media::VideoRateControlMode::CBR;
    if (normalized == "vbr") return media::VideoRateControlMode::VBR;
    if (normalized == "crf" || normalized == "cq") return media::VideoRateControlMode::CRF;
    if (normalized == "capped-vbr" || normalized == "capped_vbr" || normalized == "cvbr") {
        return media::VideoRateControlMode::CappedVBR;
    }
    throw std::runtime_error("unsupported video rate control mode: " + value);
}

media::VideoSpeedPreset parseSpeedPreset(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "ultrafast") return media::VideoSpeedPreset::Ultrafast;
    if (normalized == "superfast") return media::VideoSpeedPreset::Superfast;
    if (normalized == "veryfast") return media::VideoSpeedPreset::Veryfast;
    if (normalized == "faster") return media::VideoSpeedPreset::Faster;
    if (normalized == "fast") return media::VideoSpeedPreset::Fast;
    if (normalized == "medium") return media::VideoSpeedPreset::Medium;
    if (normalized == "slow") return media::VideoSpeedPreset::Slow;
    if (normalized == "slower") return media::VideoSpeedPreset::Slower;
    if (normalized == "veryslow" || normalized == "very-slow") return media::VideoSpeedPreset::Veryslow;
    if (normalized == "placebo") return media::VideoSpeedPreset::Placebo;
    throw std::runtime_error("unsupported video speed preset: " + value);
}

void printUsage(const char* exe)
{
    std::cout
        << "Usage:\n"
        << "  " << exe << " --input <url-or-path> [options]\n\n"
        << "Input options:\n"
        << "  -i, --input <url-or-path>       RTSP/RTP/UDP URL or local media path\n"
        << "      --duration <seconds>        Read duration before requestStop, default 10\n"
        << "      --loop <count>              Repeat start/stop cycles, default 1\n"
        << "      --open-timeout-ms <value>   Open/find-stream timeout, default 5000\n"
        << "      --read-timeout-ms <value>   av_read_frame timeout, default 5000\n"
        << "      --input-format <name>       Optional FFmpeg input format hint\n"
        << "      --accept-eof                Treat EOF as success for local-file probing\n"
        << "      --no-low-latency            Disable low-latency input options\n\n"
        << "Video transcode options:\n"
        << "      --width <value>             Output width, 0 means input width\n"
        << "      --height <value>            Output height, 0 means input height\n"
        << "      --size <WxH>                Output size, for example 1280x720\n"
        << "      --fps <value>               Output fps, 0 means preserve input timeline\n"
        << "      --video-codec <value>       h264 | h265 | mpeg4 | vp8 | vp9 | av1\n"
        << "      --bitrate <kbps>            Target video bitrate in kbps\n"
        << "      --video-bitrate <kbps>      Alias of --bitrate\n"
        << "      --min-bitrate <kbps>        Minimum video bitrate constraint\n"
        << "      --max-bitrate <kbps>        Peak video bitrate constraint\n"
        << "      --buffer-size <kbits>       Encoder VBV buffer size\n"
        << "      --rc <value>                auto | cbr | vbr | crf | capped-vbr\n"
        << "      --quality <value>           Generic quality value for crf/cq modes\n"
        << "      --preset <value>            ultrafast | superfast | veryfast | faster | fast | medium | slow | slower | veryslow | placebo\n"
        << "      --speed <value>             Alias of --preset\n"
        << "      --gop <frames>              GOP size in frames, 0 means auto\n"
        << "      --max-bframes <count>       Max B frames, default 0\n"
        << "      --tune <value>              Encoder tune if supported\n"
        << "      --profile <value>           Encoder profile if supported\n"
        << "      --level <value>             Encoder level if supported\n"
        << "      --disable-hardware          Force pure CPU decode/filter/encode path\n\n"
        << "RTP output options:\n"
        << "      --rtp-host <host>           Destination host, default 127.0.0.1\n"
        << "      --rtp-port <port>           Destination RTP port, default 5004\n"
        << "      --rtcp-port <port>          Destination RTCP port, default 0\n"
        << "      --local-rtp-port <port>     Local RTP port, default 0\n"
        << "      --local-rtcp-port <port>    Local RTCP port, default 0\n"
        << "      --packet-size <bytes>       RTP pkt_size, default 1200\n"
        << "      --sdp <path>                Write SDP file for ffplay/VLC\n\n"
        << "Other options:\n"
        << "      --verbose                   Enable debug logs\n"
        << "  -h, --help                      Show this help\n\n"
        << "This is an internal P1 tool. It validates realtime input -> decode -> encode -> RTP muxer counters.\n";
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
    options.config.videoBitrate.targetKbps = 2000;
    options.config.videoBitrate.rateControl = media::VideoRateControlMode::CBR;
    options.config.videoEncode.speedPreset = media::VideoSpeedPreset::Veryfast;
    options.config.videoEncode.maxBFrames = 0;

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
        if (arg == "--width") {
            options.config.width = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--height") {
            options.config.height = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--fps") {
            options.config.fps = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--size") {
            const std::string value = requireValue(argc, argv, i, arg);
            const std::size_t pos = value.find('x');
            if (pos == std::string::npos) {
                throw std::runtime_error("invalid value for --size: " + value);
            }
            options.config.width = parseInt(value.substr(0, pos), "--size width", 0);
            options.config.height = parseInt(value.substr(pos + 1), "--size height", 0);
            continue;
        }
        if (arg == "--video-codec") {
            options.config.videoCodec = parseVideoCodec(requireValue(argc, argv, i, arg));
            continue;
        }
        if (arg == "--bitrate" || arg == "--video-bitrate") {
            options.config.videoBitrate.targetKbps = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--min-bitrate" || arg == "--min-video-bitrate") {
            options.config.videoBitrate.minKbps = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--max-bitrate" || arg == "--max-video-bitrate") {
            options.config.videoBitrate.maxKbps = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--buffer-size" || arg == "--bufsize") {
            options.config.videoBitrate.bufferSizeKbits = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--rc" || arg == "--rate-control") {
            options.config.videoBitrate.rateControl = parseRateControl(requireValue(argc, argv, i, arg));
            continue;
        }
        if (arg == "--quality") {
            options.config.videoBitrate.quality = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--preset" || arg == "--speed") {
            options.config.videoEncode.speedPreset = parseSpeedPreset(requireValue(argc, argv, i, arg));
            continue;
        }
        if (arg == "--gop" || arg == "--gop-size") {
            options.config.videoEncode.gopSize = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--max-bframes" || arg == "--bframes") {
            options.config.videoEncode.maxBFrames = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--tune") {
            options.config.videoEncode.tune = requireValue(argc, argv, i, arg);
            continue;
        }
        if (arg == "--profile") {
            options.config.videoEncode.profile = requireValue(argc, argv, i, arg);
            continue;
        }
        if (arg == "--level") {
            options.config.videoEncode.level = requireValue(argc, argv, i, arg);
            continue;
        }
        if (arg == "--disable-hardware") {
            options.config.hardware.enabled = false;
            continue;
        }
        if (arg == "--rtp-host") {
            options.config.rtpOutput.host = requireValue(argc, argv, i, arg);
            continue;
        }
        if (arg == "--rtp-port") {
            options.config.rtpOutput.rtpPort = parseInt(requireValue(argc, argv, i, arg), arg, 1);
            continue;
        }
        if (arg == "--rtcp-port") {
            options.config.rtpOutput.rtcpPort = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--local-rtp-port") {
            options.config.rtpOutput.localRtpPort = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--local-rtcp-port") {
            options.config.rtpOutput.localRtcpPort = parseInt(requireValue(argc, argv, i, arg), arg, 0);
            continue;
        }
        if (arg == "--packet-size") {
            options.config.rtpOutput.packetSize = parseInt(requireValue(argc, argv, i, arg), arg, 1);
            continue;
        }
        if (arg == "--sdp") {
            options.config.rtpOutput.sdpOutputPath = requireValue(argc, argv, i, arg);
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
    if (stats.writtenRtpPacketCount <= 0) {
        spdlog::error("probe failed: no encoded video packets were written to RTP muxer");
        return false;
    }
    return true;
}

bool runProbeOnce(const Options& options, int index)
{
    spdlog::info("P1 realtime RTP probe iteration {}/{}", index, options.loopCount);

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
        spdlog::info(
            "input={}, duration={}s, loop={}, size={}x{}, fps={}, bitrate={}kbps, rc={}, rtp={}:{}, sdp={}, lowLatency={}, hw={}",
            options.config.inputUrl,
            options.durationSeconds,
            options.loopCount,
            options.config.width,
            options.config.height,
            options.config.fps,
            options.config.videoBitrate.targetKbps,
            static_cast<int>(options.config.videoBitrate.rateControl),
            options.config.rtpOutput.host,
            options.config.rtpOutput.rtpPort,
            options.config.rtpOutput.sdpOutputPath.empty()
                ? "disabled"
                : options.config.rtpOutput.sdpOutputPath,
            options.config.lowLatency ? "true" : "false",
            options.config.hardware.enabled ? "enabled" : "disabled"
        );

        bool ok = true;
        for (int i = 1; i <= options.loopCount; ++i) {
            ok = runProbeOnce(options, i) && ok;
        }

        if (!ok) {
            spdlog::error("P1 realtime RTP probe failed");
            return 1;
        }

        spdlog::info("P1 realtime RTP probe succeeded");
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "p1 realtime probe error: " << e.what() << '\n';
        return 1;
    }
}
