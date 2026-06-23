#include "media_transcode/FFmpegTranscoder.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

namespace {

struct CliOptions {
    media::TranscodeConfig config;
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
    if (normalized == "capped-vbr" || normalized == "capped_vbr" || normalized == "cvbr") return media::VideoRateControlMode::CappedVBR;
    throw std::runtime_error("unsupported video rate control mode: " + value);
}

media::VideoEncodeSpeedPreset parseSpeedPreset(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "ultrafast") return media::VideoEncodeSpeedPreset::Ultrafast;
    if (normalized == "superfast") return media::VideoEncodeSpeedPreset::Superfast;
    if (normalized == "veryfast") return media::VideoEncodeSpeedPreset::Veryfast;
    if (normalized == "faster") return media::VideoEncodeSpeedPreset::Faster;
    if (normalized == "fast") return media::VideoEncodeSpeedPreset::Fast;
    if (normalized == "medium") return media::VideoEncodeSpeedPreset::Medium;
    if (normalized == "slow") return media::VideoEncodeSpeedPreset::Slow;
    if (normalized == "slower") return media::VideoEncodeSpeedPreset::Slower;
    if (normalized == "veryslow" || normalized == "very-slow") return media::VideoEncodeSpeedPreset::Veryslow;
    if (normalized == "placebo") return media::VideoEncodeSpeedPreset::Placebo;
    throw std::runtime_error("unsupported video speed preset: " + value);
}

media::AudioCodec parseAudioCodec(const std::string& value)
{
    const std::string normalized = toLower(value);
    if (normalized == "auto" || normalized == "copy") return media::AudioCodec::Auto;
    if (normalized == "aac") return media::AudioCodec::AAC;
    if (normalized == "opus" || normalized == "libopus") return media::AudioCodec::OPUS;
    if (normalized == "mp3" || normalized == "libmp3lame") return media::AudioCodec::MP3;
    throw std::runtime_error("unsupported audio codec: " + value);
}

const char* videoCodecName(media::VideoCodec codec)
{
    switch (codec) {
    case media::VideoCodec::H264: return "h264";
    case media::VideoCodec::H265: return "h265";
    case media::VideoCodec::MPEG4: return "mpeg4";
    case media::VideoCodec::VP8: return "vp8";
    case media::VideoCodec::VP9: return "vp9";
    case media::VideoCodec::AV1: return "av1";
    case media::VideoCodec::Copy:
    default: return "copy";
    }
}

const char* rateControlName(media::VideoRateControlMode mode)
{
    switch (mode) {
    case media::VideoRateControlMode::CBR: return "cbr";
    case media::VideoRateControlMode::VBR: return "vbr";
    case media::VideoRateControlMode::CRF: return "crf";
    case media::VideoRateControlMode::CappedVBR: return "capped-vbr";
    case media::VideoRateControlMode::Auto:
    default: return "auto";
    }
}

const char* audioCodecName(media::AudioCodec codec)
{
    switch (codec) {
    case media::AudioCodec::AAC: return "aac";
    case media::AudioCodec::OPUS: return "opus";
    case media::AudioCodec::MP3: return "mp3";
    case media::AudioCodec::Auto:
    default: return "auto";
    }
}

std::string ffErrorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buffer, sizeof(buffer));
    return std::string(buffer) + " (" + std::to_string(err) + ")";
}

std::string formatBitrate(int64_t bitRate)
{
    if (bitRate <= 0) {
        return "unknown";
    }
    std::ostringstream oss;
    oss << bitRate / 1000 << " kbps";
    return oss.str();
}

int audioChannels(const AVCodecParameters* codecParameters)
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return codecParameters ? codecParameters->ch_layout.nb_channels : 0;
#else
    return codecParameters ? codecParameters->channels : 0;
#endif
}

std::string codecName(AVCodecID codecId)
{
    const AVCodecDescriptor* descriptor = avcodec_descriptor_get(codecId);
    if (descriptor && descriptor->name) {
        return descriptor->name;
    }
    const char* name = avcodec_get_name(codecId);
    return name ? name : "unknown";
}

void printUsage(const char* executable)
{
    std::cout
        << "Usage:\n"
        << "  " << executable << " [options]\n\n"
        << "Required options:\n"
        << "  -i, --input <path/url>          Input media path or URL\n"
        << "  -o, --output <path/url>         Output media path or URL\n\n"
        << "Video output options:\n"
        << "      --width <value>             Output width, 0 means input width\n"
        << "      --height <value>            Output height, 0 means input height\n"
        << "      --size <WxH>                Output size, for example 1280x720\n"
        << "      --fps <value>               Output fps, 0 means preserve input timeline\n"
        << "      --video-codec <value>       h264 | h265 | mpeg4 | vp8 | vp9 | av1\n"
        << "      --video-bitrate <kbps>      Target video bitrate in kbps\n"
        << "      --min-video-bitrate <kbps>  Minimum video bitrate constraint\n"
        << "      --max-video-bitrate <kbps>  Peak video bitrate constraint\n"
        << "      --buffer-size <kbits>       Encoder VBV buffer size\n"
        << "      --rc <value>                auto | cbr | vbr | crf | capped-vbr\n"
        << "      --quality <value>           Generic quality value for crf/cq modes\n"
        << "      --speed <value>             ultrafast | superfast | veryfast | faster | fast | medium | slow | slower | veryslow | placebo\n"
        << "      --gop <frames>              GOP size in frames, 0 means auto\n"
        << "      --bframes <count>           Max B frames, default 0\n"
        << "      --tune <value>              Encoder tune if supported\n"
        << "      --profile <value>           Encoder profile if supported\n"
        << "      --level <value>             Encoder level if supported\n\n"
        << "Hardware options:\n"
        << "      --enable-hardware           Enable automatic lowest-CPU hardware planning (default)\n"
        << "      --disable-hardware          Force pure CPU decode/filter/encode path\n\n"
        << "Audio output options:\n"
        << "      --audio-codec <value>       auto | aac | opus | mp3, default auto\n"
        << "      --audio-bitrate <kbps>      0 keeps input bitrate, >0 requests target bitrate\n"
        << "      --no-audio                  Disable audio output\n"
        << "  -h, --help                      Show this help\n\n";
}

CliOptions parseOptions(int argc, char* argv[])
{
    CliOptions options;
    options.config.inputUrl = "video_ornament_mingwang_v1.mp4";
    options.config.outputUrl = "output.mp4";
    options.config.width = 1280;
    options.config.height = 720;
    options.config.videoBitrate.targetKbps = 3000;

    int positionalIndex = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { options.showHelp = true; return options; }
        if (arg == "-i" || arg == "--input") { options.config.inputUrl = requireValue(argc, argv, i, arg); continue; }
        if (arg == "-o" || arg == "--output") { options.config.outputUrl = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--width") { options.config.width = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--height") { options.config.height = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--fps") { options.config.fps = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--video-codec") { options.config.videoCodec = parseVideoCodec(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--video-bitrate") { options.config.videoBitrate.targetKbps = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--min-video-bitrate") { options.config.videoBitrate.minKbps = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--max-video-bitrate") { options.config.videoBitrate.maxKbps = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--buffer-size" || arg == "--bufsize") { options.config.videoBitrate.bufferSizeKbits = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--rc" || arg == "--rate-control") { options.config.videoBitrate.rateControl = parseRateControl(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--quality") { options.config.videoBitrate.quality = parsePositiveInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--speed") { options.config.videoEncode.speedPreset = parseSpeedPreset(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--gop" || arg == "--gop-size") { options.config.videoEncode.gopSize = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--bframes" || arg == "--max-bframes") { options.config.videoEncode.maxBFrames = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--tune") { options.config.videoEncode.tune = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--profile") { options.config.videoEncode.profile = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--level") { options.config.videoEncode.level = requireValue(argc, argv, i, arg); continue; }
        if (arg == "--enable-hardware") { options.config.hardware.enabled = true; continue; }
        if (arg == "--disable-hardware") { options.config.hardware.enabled = false; continue; }
        if (arg == "--audio-codec") { options.config.audioEnabled = true; options.config.audioCodec = parseAudioCodec(requireValue(argc, argv, i, arg)); continue; }
        if (arg == "--audio-bitrate") { options.config.audioEnabled = true; options.config.audioBitrateKbps = parseNonNegativeInt(requireValue(argc, argv, i, arg), arg); continue; }
        if (arg == "--no-audio") { options.config.audioEnabled = false; continue; }
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
            options.config.inputUrl = arg;
        }
        else if (positionalIndex == 1) {
            options.config.outputUrl = arg;
        }
        else {
            throw std::runtime_error("too many positional arguments: " + arg);
        }
        ++positionalIndex;
    }
    return options;
}

bool printMediaInfo(const std::string& url, const std::string& title)
{
    AVFormatContext* formatContext = nullptr;
    int ret = avformat_open_input(&formatContext, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        spdlog::error("{}: avformat_open_input failed: {}", title, ffErrorString(ret));
        return false;
    }

    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        spdlog::error("{}: avformat_find_stream_info failed: {}", title, ffErrorString(ret));
        avformat_close_input(&formatContext);
        return false;
    }

    spdlog::info("========== {} =========", title);
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        const AVStream* stream = formatContext->streams[i];
        const AVCodecParameters* codecParameters = stream ? stream->codecpar : nullptr;
        if (!codecParameters) {
            continue;
        }
        if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            const char* pixelFormatName = codecParameters->format >= 0
                ? av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecParameters->format))
                : nullptr;
            spdlog::info("stream #{}: video, codec={}, {}x{}, pix_fmt={}, bitrate={}",
                         i,
                         codecName(codecParameters->codec_id),
                         codecParameters->width,
                         codecParameters->height,
                         pixelFormatName ? pixelFormatName : "unknown",
                         formatBitrate(codecParameters->bit_rate));
        }
        else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            spdlog::info("stream #{}: audio, codec={}, sample_rate={} Hz, channels={}, bitrate={}",
                         i,
                         codecName(codecParameters->codec_id),
                         codecParameters->sample_rate,
                         audioChannels(codecParameters),
                         formatBitrate(codecParameters->bit_rate));
        }
    }

    avformat_close_input(&formatContext);
    return true;
}

void logConfig(const media::TranscodeConfig& config)
{
    spdlog::info("========== Transcode Config ==========");
    spdlog::info("input={}", config.inputUrl);
    spdlog::info("output={}", config.outputUrl);
    spdlog::info("size={}x{}", config.width, config.height);
    spdlog::info("fps={}", config.fps);
    spdlog::info("videoCodec={}", videoCodecName(config.videoCodec));
    spdlog::info("videoBitrateTarget={} kbps", config.videoBitrate.targetKbps);
    spdlog::info("videoRateControl={}", rateControlName(config.videoBitrate.rateControl));
    spdlog::info("audioEnabled={}", config.audioEnabled);
    spdlog::info("audioCodecTarget={}", audioCodecName(config.audioCodec));
    spdlog::info("audioBitrateTarget={} kbps", config.audioBitrateKbps);
    spdlog::info("hardwareEnabled={}", config.hardware.enabled);
}

} // namespace

int main(int argc, char* argv[])
{
    auto logger = spdlog::stdout_color_mt("media_transcode_cli");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    avformat_network_init();

    CliOptions options;
    try {
        options = parseOptions(argc, argv);
    }
    catch (const std::exception& e) {
        spdlog::error("{}", e.what());
        printUsage(argv[0]);
        spdlog::shutdown();
        return 1;
    }

    if (options.showHelp) {
        printUsage(argv[0]);
        spdlog::shutdown();
        return 0;
    }

    logConfig(options.config);
    if (!printMediaInfo(options.config.inputUrl, "Input Media Info")) {
        spdlog::shutdown();
        return 1;
    }

    media::FFmpegTranscoder transcoder;
    transcoder.setProgressCallback([](const media::ProgressInfo& info) {
        spdlog::info("Progress: frame={}, outTimeMs={}, speed={}x, state={}",
                     info.frame,
                     info.outTimeMs,
                     info.speed,
                     info.raw);
    });

    if (!transcoder.initialize(options.config)) {
        spdlog::error("Transcoder initialize failed: {}", transcoder.lastError());
        spdlog::shutdown();
        return 1;
    }

    if (!transcoder.start()) {
        spdlog::error("Transcoder start failed: {}", transcoder.lastError());
        spdlog::shutdown();
        return 1;
    }

    transcoder.wait();
    if (!transcoder.lastError().empty()) {
        spdlog::error("Transcode failed: {}", transcoder.lastError());
        spdlog::shutdown();
        return 1;
    }

    spdlog::info("Transcode done");
    if (!printMediaInfo(options.config.outputUrl, "Output Media Info")) {
        spdlog::shutdown();
        return 1;
    }

    spdlog::shutdown();
    return 0;
}
