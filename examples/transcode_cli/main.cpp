#include "media_transcode/FFmpegTranscoder.h"

#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
}

namespace {

    struct CliOptions {
        media::TranscodeConfig config;
        bool showHelp = false;
    };

    std::string ffErrorString(int err)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(err, buffer, sizeof(buffer));

        std::ostringstream oss;
        oss << buffer << " (" << err << ")";
        return oss.str();
    }

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

    media::VideoCodec parseVideoCodec(const std::string& value)
    {
        const std::string normalized = toLower(value);

        if (normalized == "copy") {
            return media::VideoCodec::Copy;
        }

        if (normalized == "h264" || normalized == "avc") {
            return media::VideoCodec::H264;
        }

        if (normalized == "h265" || normalized == "hevc") {
            return media::VideoCodec::H265;
        }

        if (normalized == "mpeg4" || normalized == "mp4v") {
            return media::VideoCodec::MPEG4;
        }

        if (normalized == "vp8") {
            return media::VideoCodec::VP8;
        }

        if (normalized == "vp9") {
            return media::VideoCodec::VP9;
        }

        if (normalized == "av1") {
            return media::VideoCodec::AV1;
        }

        throw std::runtime_error("unsupported video codec: " + value);
    }

    media::AudioCodec parseAudioCodec(const std::string& value)
    {
        const std::string normalized = toLower(value);

        if (normalized == "aac") {
            return media::AudioCodec::AAC;
        }

        if (normalized == "opus" || normalized == "libopus") {
            return media::AudioCodec::OPUS;
        }

        if (normalized == "mp3" || normalized == "libmp3lame") {
            return media::AudioCodec::MP3;
        }

        throw std::runtime_error("unsupported audio codec: " + value);
    }

    std::string videoCodecToString(media::VideoCodec codec)
    {
        switch (codec) {
        case media::VideoCodec::H264:
            return "h264";
        case media::VideoCodec::H265:
            return "h265";
        case media::VideoCodec::MPEG4:
            return "mpeg4";
        case media::VideoCodec::VP8:
            return "vp8";
        case media::VideoCodec::VP9:
            return "vp9";
        case media::VideoCodec::AV1:
            return "av1";
        case media::VideoCodec::Copy:
        default:
            return "copy";
        }
    }

    std::string audioCodecToString(media::AudioCodec codec)
    {
        switch (codec) {
        case media::AudioCodec::AAC:
            return "aac";
        case media::AudioCodec::OPUS:
            return "opus";
        case media::AudioCodec::MP3:
            return "mp3";
        default:
            return "unknown";
        }
    }

    std::string audioModeToString(media::AudioMode mode)
    {
        switch (mode) {
        case media::AudioMode::None:
            return "none";
        case media::AudioMode::CopySelected:
            return "copy";
        case media::AudioMode::EncodeSelected:
            return "encode";
        default:
            return "unknown";
        }
    }

    std::string formatDuration(int64_t durationUs)
    {
        if (durationUs == AV_NOPTS_VALUE || durationUs < 0) {
            return "unknown";
        }

        const double totalSeconds = durationUs / 1000000.0;
        const int64_t integerSeconds = static_cast<int64_t>(totalSeconds);
        const int64_t hours = integerSeconds / 3600;
        const int64_t minutes = (integerSeconds % 3600) / 60;
        const double seconds = totalSeconds - static_cast<double>(hours * 3600 + minutes * 60);

        std::ostringstream oss;
        oss << hours << ":"
            << std::setw(2) << std::setfill('0') << minutes << ":"
            << std::fixed << std::setprecision(3)
            << std::setw(6) << std::setfill('0') << seconds;

        return oss.str();
    }

    std::string formatBitrate(int64_t bitRate)
    {
        if (bitRate <= 0) {
            return "unknown";
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << bitRate / 1000.0 << " kbps";
        return oss.str();
    }

    std::string formatRational(AVRational value)
    {
        if (value.num <= 0 || value.den <= 0) {
            return "unknown";
        }

        std::ostringstream oss;
        oss << value.num << "/" << value.den
            << " (" << std::fixed << std::setprecision(3) << av_q2d(value) << ")";
        return oss.str();
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

    int audioChannels(const AVCodecParameters* codecParameters)
    {
        if (!codecParameters) {
            return 0;
        }

#if LIBAVUTIL_VERSION_MAJOR >= 57
        return codecParameters->ch_layout.nb_channels;
#else
        return codecParameters->channels;
#endif
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
            << "      --video-bitrate <kbps>      Video bitrate in kbps\n"
            << "      --no-zero-copy-fallback     Fail if automatic zero-copy planning is unavailable\n\n"
            << "Audio output options:\n"
            << "      --audio-codec <value>       aac | opus | mp3\n"
            << "      --audio-bitrate <kbps>      Audio bitrate in kbps\n"
            << "      --no-audio                  Disable audio output\n"
            << "  -h, --help                      Show this help\n\n"
            << "Examples:\n"
            << "  " << executable << " -i input.mp4 -o output.mp4 --video-codec h264 --size 1280x720 --fps 25 --video-bitrate 3000 --audio-codec aac --audio-bitrate 128\n"
            << "  " << executable << " -i input.mp4 -o output_strict.mp4 --video-codec h264 --size 1280x720 --fps 25 --video-bitrate 3000 --no-zero-copy-fallback\n";
    }

    CliOptions parseOptions(int argc, char* argv[])
    {
        CliOptions options;

        options.config.inputUrl = "video_ornament_mingwang_v1.mp4";
        options.config.outputUrl = "output.mp4";
        options.config.width = 1280;
        options.config.height = 720;
        options.config.fps = 0;
        options.config.videoCodec = media::VideoCodec::H264;
        options.config.audioMode = media::AudioMode::EncodeSelected;
        options.config.audioCodec = media::AudioCodec::AAC;
        options.config.audioBitrateKbps = 128;
        options.config.videoBitrateKbps = 3000;
        options.config.hardware.allowZeroCopyFallback = true;

        int positionalIndex = 0;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];

            auto requireValue = [&](const std::string& optionName) -> std::string {
                if (i + 1 >= argc) {
                    throw std::runtime_error("missing value for " + optionName);
                }

                return argv[++i];
            };

            if (arg == "-h" || arg == "--help") {
                options.showHelp = true;
                return options;
            }
            else if (arg == "-i" || arg == "--input") {
                options.config.inputUrl = requireValue(arg);
            }
            else if (arg == "-o" || arg == "--output") {
                options.config.outputUrl = requireValue(arg);
            }
            else if (arg == "--width") {
                options.config.width = parseNonNegativeInt(requireValue(arg), arg);
            }
            else if (arg == "--height") {
                options.config.height = parseNonNegativeInt(requireValue(arg), arg);
            }
            else if (arg == "--size") {
                const std::string value = requireValue(arg);
                const std::size_t pos = value.find('x');

                if (pos == std::string::npos) {
                    throw std::runtime_error("invalid value for --size: " + value);
                }

                options.config.width = parseNonNegativeInt(value.substr(0, pos), "--size width");
                options.config.height = parseNonNegativeInt(value.substr(pos + 1), "--size height");
            }
            else if (arg == "--fps") {
                options.config.fps = parseNonNegativeInt(requireValue(arg), arg);
            }
            else if (arg == "--video-codec") {
                options.config.videoCodec = parseVideoCodec(requireValue(arg));
            }
            else if (arg == "--video-bitrate") {
                options.config.videoBitrateKbps = parsePositiveInt(requireValue(arg), arg);
            }
            else if (arg == "--audio-codec") {
                options.config.audioCodec = parseAudioCodec(requireValue(arg));
                if (options.config.audioMode == media::AudioMode::None) {
                    options.config.audioMode = media::AudioMode::EncodeSelected;
                }
            }
            else if (arg == "--audio-bitrate") {
                options.config.audioBitrateKbps = parsePositiveInt(requireValue(arg), arg);
            }
            else if (arg == "--no-audio") {
                options.config.audioMode = media::AudioMode::None;
            }
            else if (arg == "--no-zero-copy-fallback") {
                options.config.hardware.allowZeroCopyFallback = false;
            }
            else if (arg == "--allow-zero-copy-fallback") {
                options.config.hardware.allowZeroCopyFallback = true;
            }
            else if (!arg.empty() && arg[0] == '-') {
                throw std::runtime_error("unknown option: " + arg);
            }
            else {
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
        spdlog::info("file: {}", url);
        spdlog::info(
            "format={}, duration={}, bitrate={}",
            formatContext->iformat && formatContext->iformat->name ? formatContext->iformat->name : "unknown",
            formatDuration(formatContext->duration),
            formatBitrate(formatContext->bit_rate)
        );

        for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
            AVStream* stream = formatContext->streams[i];
            if (!stream || !stream->codecpar) {
                continue;
            }

            const AVCodecParameters* codecParameters = stream->codecpar;
            const char* mediaType = av_get_media_type_string(codecParameters->codec_type);

            int64_t streamDurationUs = AV_NOPTS_VALUE;
            if (stream->duration != AV_NOPTS_VALUE) {
                streamDurationUs = av_rescale_q(
                    stream->duration,
                    stream->time_base,
                    AV_TIME_BASE_Q
                );
            }

            if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
                const char* pixelFormatName = codecParameters->format >= 0
                    ? av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecParameters->format))
                    : nullptr;

                const AVRational guessedFrameRate = av_guess_frame_rate(formatContext, stream, nullptr);

                spdlog::info(
                    "stream #{}: type={}, codec={}, {}x{}, pix_fmt={}, fps={}, time_base={}, bitrate={}, duration={}",
                    i,
                    mediaType ? mediaType : "video",
                    codecName(codecParameters->codec_id),
                    codecParameters->width,
                    codecParameters->height,
                    pixelFormatName ? pixelFormatName : "unknown",
                    formatRational(guessedFrameRate),
                    formatRational(stream->time_base),
                    formatBitrate(codecParameters->bit_rate),
                    formatDuration(streamDurationUs)
                );
            }
            else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
                const char* sampleFormatName = codecParameters->format >= 0
                    ? av_get_sample_fmt_name(static_cast<AVSampleFormat>(codecParameters->format))
                    : nullptr;

                spdlog::info(
                    "stream #{}: type={}, codec={}, sample_rate={} Hz, channels={}, sample_fmt={}, time_base={}, bitrate={}, duration={}",
                    i,
                    mediaType ? mediaType : "audio",
                    codecName(codecParameters->codec_id),
                    codecParameters->sample_rate,
                    audioChannels(codecParameters),
                    sampleFormatName ? sampleFormatName : "unknown",
                    formatRational(stream->time_base),
                    formatBitrate(codecParameters->bit_rate),
                    formatDuration(streamDurationUs)
                );
            }
        }

        avformat_close_input(&formatContext);
        return true;
    }

    void logTranscodeConfig(const media::TranscodeConfig& config)
    {
        spdlog::info("========== Transcode Config ==========");
        spdlog::info("input={}", config.inputUrl);
        spdlog::info("output={}", config.outputUrl);
        spdlog::info("size={}x{}", config.width, config.height);
        spdlog::info("fps={}", config.fps);
        spdlog::info("videoCodec={}", videoCodecToString(config.videoCodec));
        spdlog::info("videoBitrate={} kbps", config.videoBitrateKbps);
        spdlog::info("audioMode={}", audioModeToString(config.audioMode));
        spdlog::info("audioCodec={}", audioCodecToString(config.audioCodec));
        spdlog::info("audioBitrate={} kbps", config.audioBitrateKbps);
        spdlog::info("zeroCopyPreferred=true");
        spdlog::info("zeroCopyFallbackAllowed={}", config.hardware.allowZeroCopyFallback);
    }

    void initLogger()
    {
        std::filesystem::create_directories("logs");

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

        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::info);
        spdlog::flush_every(std::chrono::seconds(3));
    }

} // namespace

int main(int argc, char* argv[])
{
    initLogger();
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

    logTranscodeConfig(options.config);

    if (!printMediaInfo(options.config.inputUrl, "Input Media Info")) {
        spdlog::shutdown();
        return 1;
    }

    media::FFmpegTranscoder transcoder;
    transcoder.setProgressCallback([](const media::ProgressInfo& info) {
        spdlog::info(
            "Progress: frame={}, outTimeMs={}, speed={}x, state={}",
            info.frame,
            info.outTimeMs,
            info.speed,
            info.raw
        );
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
