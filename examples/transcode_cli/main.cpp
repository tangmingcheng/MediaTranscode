#include "media_transcode/MediaTranscode.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct CliOptions {
    media::LocalVideoTranscodeConfig config;
    bool showHelp = false;
};

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

int parseNonNegativeInt(const std::string& value, const std::string& name)
{
    try {
        std::size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size() || result < 0) {
            throw std::invalid_argument("invalid integer");
        }
        return result;
    }
    catch (const std::exception&) {
        throw std::runtime_error("invalid value for " + name + ": " + value);
    }
}

int parsePositiveInt(const std::string& value, const std::string& name)
{
    const int result = parseNonNegativeInt(value, name);
    if (result <= 0) {
        throw std::runtime_error("invalid value for " + name + ": " + value);
    }
    return result;
}

std::string requireValue(int argc, char* argv[], int& index, const std::string& optionName)
{
    if (index + 1 >= argc) {
        throw std::runtime_error("missing value for " + optionName);
    }
    return argv[++index];
}

media::OutputVideoCodec parseVideoCodec(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "h264" || normalized == "avc") return media::OutputVideoCodec::H264;
    if (normalized == "h265" || normalized == "hevc") return media::OutputVideoCodec::H265;
    if (normalized == "mpeg4" || normalized == "mp4v") return media::OutputVideoCodec::MPEG4;
    if (normalized == "vp8") return media::OutputVideoCodec::VP8;
    if (normalized == "vp9") return media::OutputVideoCodec::VP9;
    if (normalized == "av1") return media::OutputVideoCodec::AV1;
    throw std::runtime_error("unsupported video codec: " + value);
}

media::VideoRcMode parseRateControl(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "auto") return media::VideoRcMode::Auto;
    if (normalized == "cbr") return media::VideoRcMode::CBR;
    if (normalized == "vbr") return media::VideoRcMode::VBR;
    if (normalized == "crf" || normalized == "cq") return media::VideoRcMode::CRF;
    if (normalized == "capped-vbr" || normalized == "capped_vbr" || normalized == "cvbr") {
        return media::VideoRcMode::CappedVBR;
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

media::OutputAudioCodec parseAudioCodec(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "auto" || normalized == "copy") return media::OutputAudioCodec::Auto;
    if (normalized == "aac") return media::OutputAudioCodec::AAC;
    if (normalized == "opus" || normalized == "libopus") return media::OutputAudioCodec::OPUS;
    if (normalized == "mp3" || normalized == "libmp3lame") return media::OutputAudioCodec::MP3;
    throw std::runtime_error("unsupported audio codec: " + value);
}

void printUsage(const char* executable)
{
    std::cout
        << "Usage:\n"
        << "  " << executable << " [options]\n\n"
        << "Required options:\n"
        << "  -i, --input <path>             Local input video file path\n"
        << "  -o, --output <path>            Local output video file path\n\n"
        << "Video output options:\n"
        << "      --width <value>            Output width, 0 means input width\n"
        << "      --height <value>           Output height, 0 means input height\n"
        << "      --size <WxH>               Output size, for example 1280x720\n"
        << "      --fps <value>              Output fps, 0 means preserve input timeline\n"
        << "      --video-codec <value>      h264 | h265 | mpeg4 | vp8 | vp9 | av1\n"
        << "      --video-bitrate <kbps>     Target video bitrate in kbps\n"
        << "      --min-video-bitrate <kbps> Minimum video bitrate constraint\n"
        << "      --max-video-bitrate <kbps> Peak video bitrate constraint\n"
        << "      --buffer-size <kbits>      Encoder VBV buffer size\n"
        << "      --rc <value>               auto | cbr | vbr | crf | capped-vbr\n"
        << "      --quality <value>          Generic quality value for crf/cq modes\n"
        << "      --speed <value>            ultrafast | superfast | veryfast | faster | fast | medium | slow | slower | veryslow | placebo\n"
        << "      --gop <frames>             GOP size in frames, 0 means auto\n"
        << "      --bframes <count>          Max B frames, default 0\n"
        << "      --tune <value>             Encoder tune if supported\n"
        << "      --profile <value>          Encoder profile if supported\n"
        << "      --level <value>            Encoder level if supported\n\n"
        << "Hardware options:\n"
        << "      --enable-hardware          Enable automatic hardware planning (default)\n"
        << "      --disable-hardware         Force pure CPU decode/filter/encode path\n\n"
        << "Audio output options:\n"
        << "      --audio-codec <value>      auto | aac | opus | mp3, default auto\n"
        << "      --audio-bitrate <kbps>     0 keeps input bitrate, >0 requests target bitrate\n"
        << "      --no-audio                 Disable audio output\n"
        << "  -h, --help                     Show this help\n\n";
}

CliOptions parseOptions(int argc, char* argv[])
{
    CliOptions options;
    options.config.inputPath = "video_ornament_mingwang_v1.mp4";
    options.config.outputPath = "output.mp4";
    options.config.width = 1280;
    options.config.height = 720;
    options.config.videoBitrateKbps = 3000;

    int positionalIndex = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { options.showHelp = true; return options; }
        if (arg == "-i" || arg == "--input") { options.config.inputPath = requireValue(argc, argv, i, arg); continue; }
        if (arg == "-o" || arg == "--output") { options.config.outputPath = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--width") { options.config.width = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--height") { options.config.height = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--fps") { options.config.fps = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--video-codec") { options.config.videoCodec = parseVideoCodec(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--video-bitrate") { options.config.videoBitrateKbps = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--min-video-bitrate") { options.config.minVideoBitrateKbps = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--max-video-bitrate") { options.config.maxVideoBitrateKbps = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--buffer-size" || arg == "--bufsize") { options.config.videoBufferSizeKbits = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--rc" || arg == "--rate-control") { options.config.rcMode = parseRateControl(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--quality") { options.config.quality = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--speed") { options.config.speed = parseSpeedPreset(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--gop" || arg == "--gop-size") { options.config.gopSize = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--bframes" || arg == "--max-bframes") { options.config.maxBFrames = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--tune") { options.config.tune = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--profile") { options.config.profile = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--level") { options.config.level = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--enable-hardware") { options.config.disableHardware = false; continue; }
        if (arg == "--disable-hardware") { options.config.disableHardware = true; continue; }
        if (arg == "--audio-codec") { options.config.noAudio = false; options.config.audioCodec = parseAudioCodec(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--audio-bitrate") { options.config.noAudio = false; options.config.audioBitrateKbps = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--no-audio") { options.config.noAudio = true; continue; }
        if (arg == "--size") {
            const std::string value = requireValue(argc, argv, i, arg);
            const std::size_t pos = value.find('x');
            if (pos == std::string::npos) {
                throw std::runtime_error("invalid value for --size: " + value);
            }
            options.config.width = parseNonNegativeInt(value.substr(0, pos), "--size width");
            options.config.height = parseNonNegativeInt(value.substr(pos + 1), "--size height");
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }
        if (positionalIndex == 0) {
            options.config.inputPath = arg;
        }
        else if (positionalIndex == 1) {
            options.config.outputPath = arg;
        }
        else {
            throw std::runtime_error("too many positional arguments: " + arg);
        }
        ++positionalIndex;
    }
    return options;
}

void printConfig(const media::LocalVideoTranscodeConfig& config)
{
    spdlog::info("input:  {}", config.inputPath);
    spdlog::info("output: {}", config.outputPath);
    spdlog::info("size:   {}x{} (0 keeps input)", config.width, config.height);
    spdlog::info("fps:    {} (0 keeps input)", config.fps);
    spdlog::info("hw:     {}", config.disableHardware ? "disabled" : "enabled");
    spdlog::info("audio:  {}", config.noAudio ? "disabled" : "enabled");
}

} // namespace

int main(int argc, char* argv[])
{
    auto logger = spdlog::stdout_color_mt("media_transcode_cli");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    try {
        const CliOptions options = parseOptions(argc, argv);
        if (options.showHelp) {
            printUsage(argv[0]);
            return 0;
        }

        printConfig(options.config);

        auto result = media::startLocalVideoTranscodeSync(
            options.config,
            [](const media::LocalVideoTranscodeProgress& progress) {
                spdlog::info(
                    "progress: stage={}, frame={}, outTimeMs={}, speed={:.2f}x",
                    progress.stage,
                    progress.frame,
                    progress.outTimeMs,
                    progress.speed
                );
            }
        );

        if (!result) {
            spdlog::error("transcode failed: {}", result.error().describe());
            return 1;
        }

        const media::LocalVideoTranscodeReport& report = result.value();
        spdlog::info(
            "transcode {}: frame={}, outTimeMs={}, speed={:.2f}x",
            report.stopped ? "stopped" : "completed",
            report.lastProgress.frame,
            report.lastProgress.outTimeMs,
            report.lastProgress.speed
        );
        return report.completed || report.stopped ? 0 : 1;
    }
    catch (const std::exception& e) {
        spdlog::error("{}", e.what());
        printUsage(argv[0]);
        return 1;
    }
}
