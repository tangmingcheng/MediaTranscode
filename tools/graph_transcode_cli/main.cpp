#include "internal/FFmpegRAII.h"
#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

using namespace media::ffmpeg::graph;

namespace {

struct VideoCodecBootstrap {
    ::media::ffmpeg::CodecContextPtr decoderContext;
    ::media::ffmpeg::CodecContextPtr encoderContext;
    int videoStreamIndex = invalidMediaStreamIndex;
    int sourceWidth = 0;
    int sourceHeight = 0;
    AVPixelFormat sourcePixelFormat = AV_PIX_FMT_NONE;
    AVRational frameRate{30, 1};
    AVRational timeBase{1, 30};
    std::string decoderName;
    std::string encoderName;
};

std::string ffmpegErrorString(int errorCode)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, text, sizeof(text));
    return text;
}

std::string getArg(int argc, char** argv, const std::string& key, const std::string& def = "")
{
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == key) {
            return argv[i + 1];
        }
    }
    return def;
}

bool hasArg(int argc, char** argv, const std::string& key)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return true;
        }
    }
    return false;
}

int parseInt(int argc, char** argv, const std::string& key, int def)
{
    const std::string value = getArg(argc, argv, key);
    if (value.empty()) {
        return def;
    }
    return std::atoi(value.c_str());
}

void printUsage()
{
    std::cout << "Usage:\n"
                 "  media_transcode_graph_transcode_cli --input input.mp4 --output out.mp4 [options]\n\n"
                 "Options:\n"
                 "  --format mp4|flv|mpegts|...\n"
                 "  --video-codec h264|hevc|mpeg4|...\n"
                 "  --encoder libx264|h264_mf|h264_nvenc|auto\n"
                 "  --width 1920\n"
                 "  --height 1080\n"
                 "  --fps 30\n"
                 "  --bitrate 8000              video bitrate in kbps\n"
                 "  --rc auto|cbr|vbr|crf\n"
                 "  --crf 23\n"
                 "  --quality 23\n"
                 "  --gop 60\n"
                 "  --bframes 0\n"
                 "  --preset veryfast\n"
                 "  --profile high\n"
                 "  --level 4.1\n"
                 "  --tune zerolatency\n"
                 "  --audio-codec aac|copy|auto\n"
                 "  --audio-bitrate 128         audio bitrate in kbps\n"
                 "  --sample-rate 48000\n"
                 "  --channels 2\n"
                 "  --no-video\n"
                 "  --no-audio\n"
                 "  --audio-transcode\n"
                 "  --disable-hw\n"
                 "  --max-iterations 1024\n"
                 "  --idle-threshold 16\n";
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    std::cerr << "[CLI] " << action << " failed: " << status.error().describe() << '\n';
    return 1;
}

template <typename T>
int failResult(const std::string& action, const ::media::Result<T>& result)
{
    std::cerr << "[CLI] " << action << " failed: " << result.error().describe() << '\n';
    return 1;
}

MediaNodeId findNodeByName(const MediaGraph& graph, const std::string& name)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.name == name) {
            return node.id;
        }
    }
    return MediaNodeId::invalid();
}

void addEncoderCandidate(std::vector<const AVCodec*>& candidates, const AVCodec* codec)
{
    if (!codec) {
        return;
    }
    if (std::find(candidates.begin(), candidates.end(), codec) == candidates.end()) {
        candidates.push_back(codec);
    }
}

AVCodecID codecIdFromName(const std::string& codecName)
{
    if (codecName == "h264" || codecName == "avc") {
        return AV_CODEC_ID_H264;
    }
    if (codecName == "h265" || codecName == "hevc") {
        return AV_CODEC_ID_HEVC;
    }
    if (codecName == "mpeg4") {
        return AV_CODEC_ID_MPEG4;
    }
    if (codecName == "vp9") {
        return AV_CODEC_ID_VP9;
    }
    return AV_CODEC_ID_H264;
}

std::vector<const AVCodec*> encoderCandidates(const LocalFileTranscodeOptions& options)
{
    std::vector<const AVCodec*> candidates;

    if (!options.videoEncoder.empty() && options.videoEncoder != "auto") {
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name(options.videoEncoder.c_str()));
        return candidates;
    }

    const AVCodecID codecId = codecIdFromName(options.videoCodec);
    if (codecId == AV_CODEC_ID_H264) {
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("libx264"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_mf"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_nvenc"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_qsv"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_amf"));
    } else if (codecId == AV_CODEC_ID_HEVC) {
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("libx265"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_mf"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_nvenc"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_qsv"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_amf"));
    }

    addEncoderCandidate(candidates, avcodec_find_encoder(codecId));
    return candidates;
}

bool pixelFormatSupported(const AVCodec* codec, AVPixelFormat format)
{
    if (!codec || !codec->pix_fmts || format == AV_PIX_FMT_NONE) {
        return true;
    }
    for (const AVPixelFormat* current = codec->pix_fmts; *current != AV_PIX_FMT_NONE; ++current) {
        if (*current == format) {
            return true;
        }
    }
    return false;
}

AVPixelFormat chooseEncoderPixelFormat(const AVCodec* codec, AVPixelFormat sourceFormat)
{
    if (sourceFormat != AV_PIX_FMT_NONE && pixelFormatSupported(codec, sourceFormat)) {
        return sourceFormat;
    }
    if (pixelFormatSupported(codec, AV_PIX_FMT_YUV420P)) {
        return AV_PIX_FMT_YUV420P;
    }
    if (codec && codec->pix_fmts && codec->pix_fmts[0] != AV_PIX_FMT_NONE) {
        return codec->pix_fmts[0];
    }
    return sourceFormat != AV_PIX_FMT_NONE ? sourceFormat : AV_PIX_FMT_YUV420P;
}

void setPrivateOption(AVCodecContext* context, const std::string& key, const std::string& value)
{
    if (!context || !context->priv_data || key.empty() || value.empty()) {
        return;
    }
    av_opt_set(context->priv_data, key.c_str(), value.c_str(), 0);
}

::media::Result<VideoCodecBootstrap> createVideoCodecBootstrap(const LocalFileTranscodeOptions& options)
{
    AVFormatContext* rawFormat = nullptr;
    int ret = avformat_open_input(&rawFormat, options.inputUrl.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return ::media::Result<VideoCodecBootstrap>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_open_input: " + ffmpegErrorString(ret), ret));
    }

    ::media::ffmpeg::InputFormatContextPtr inputContext(rawFormat);
    ret = avformat_find_stream_info(inputContext.get(), nullptr);
    if (ret < 0) {
        return ::media::Result<VideoCodecBootstrap>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(ret), ret));
    }

    const int videoStreamIndex = av_find_best_stream(inputContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        return ::media::Result<VideoCodecBootstrap>::failure(
            ::media::ErrorInfo::ffmpegFailure("av_find_best_stream(video): " + ffmpegErrorString(videoStreamIndex), videoStreamIndex));
    }

    AVStream* videoStream = inputContext->streams[videoStreamIndex];
    const AVCodec* decoder = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!decoder) {
        return ::media::Result<VideoCodecBootstrap>::failure(
            ::media::ErrorInfo::unsupported("video decoder not found"));
    }

    auto decoderContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!decoderContext) {
        return ::media::Result<VideoCodecBootstrap>::failure(
            ::media::ErrorInfo::allocationFailed("avcodec_alloc_context3(decoder) returned null"));
    }

    ret = avcodec_parameters_to_context(decoderContext.get(), videoStream->codecpar);
    if (ret < 0) {
        return ::media::Result<VideoCodecBootstrap>::failure(
            ::media::ErrorInfo::ffmpegFailure("avcodec_parameters_to_context(video): " + ffmpegErrorString(ret), ret));
    }

    ret = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (ret < 0) {
        return ::media::Result<VideoCodecBootstrap>::failure(
            ::media::ErrorInfo::ffmpegFailure("avcodec_open2(decoder): " + ffmpegErrorString(ret), ret));
    }

    AVRational frameRate = av_guess_frame_rate(inputContext.get(), videoStream, nullptr);
    if (options.fpsNum > 0) {
        frameRate = AVRational{options.fpsNum, options.fpsDen > 0 ? options.fpsDen : 1};
    }
    if (frameRate.num <= 0 || frameRate.den <= 0) {
        frameRate = AVRational{30, 1};
    }

    const int encodeWidth = options.width > 0 ? options.width : decoderContext->width;
    const int encodeHeight = options.height > 0 ? options.height : decoderContext->height;
    const int64_t bitRate = options.videoBitrateKbps > 0
        ? static_cast<int64_t>(options.videoBitrateKbps) * 1000
        : (decoderContext->bit_rate > 0 ? decoderContext->bit_rate : 4'000'000);

    std::string lastEncoderError = "no encoder candidates available";
    for (const AVCodec* encoder : encoderCandidates(options)) {
        if (!encoder) {
            continue;
        }

        auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
        if (!encoderContext) {
            lastEncoderError = std::string(encoder->name ? encoder->name : "unknown") + ": avcodec_alloc_context3 returned null";
            continue;
        }

        encoderContext->width = encodeWidth;
        encoderContext->height = encodeHeight;
        encoderContext->pix_fmt = chooseEncoderPixelFormat(encoder, decoderContext->pix_fmt);
        encoderContext->time_base = AVRational{frameRate.den, frameRate.num};
        encoderContext->framerate = frameRate;
        encoderContext->bit_rate = bitRate;
        encoderContext->gop_size = options.gop > 0 ? options.gop : frameRate.num / frameRate.den * 2;
        if (encoderContext->gop_size <= 0) {
            encoderContext->gop_size = 60;
        }
        encoderContext->max_b_frames = options.maxBFrames;
        encoderContext->sample_aspect_ratio = decoderContext->sample_aspect_ratio;
        encoderContext->color_range = decoderContext->color_range;
        encoderContext->color_primaries = decoderContext->color_primaries;
        encoderContext->color_trc = decoderContext->color_trc;
        encoderContext->colorspace = decoderContext->colorspace;

        if (options.rateControlMode == "cbr" && bitRate > 0) {
            encoderContext->rc_min_rate = bitRate;
            encoderContext->rc_max_rate = bitRate;
            encoderContext->rc_buffer_size = static_cast<int>(bitRate * 2);
        } else if (options.rateControlMode == "vbr" && bitRate > 0) {
            encoderContext->rc_max_rate = bitRate;
            encoderContext->rc_buffer_size = static_cast<int>(bitRate * 2);
        }

        setPrivateOption(encoderContext.get(), "preset", options.speedPreset);
        setPrivateOption(encoderContext.get(), "profile", options.profile);
        setPrivateOption(encoderContext.get(), "tune", options.tune);
        setPrivateOption(encoderContext.get(), "level", options.level);
        if (options.crf >= 0) {
            setPrivateOption(encoderContext.get(), "crf", std::to_string(options.crf));
        }
        if (options.quality >= 0) {
            setPrivateOption(encoderContext.get(), "quality", std::to_string(options.quality));
            setPrivateOption(encoderContext.get(), "q", std::to_string(options.quality));
        }

        ret = avcodec_open2(encoderContext.get(), encoder, nullptr);
        if (ret < 0) {
            lastEncoderError = std::string(encoder->name ? encoder->name : "unknown") + ": avcodec_open2: " + ffmpegErrorString(ret);
            continue;
        }

        VideoCodecBootstrap bootstrap;
        bootstrap.decoderContext = std::move(decoderContext);
        bootstrap.encoderContext = std::move(encoderContext);
        bootstrap.videoStreamIndex = videoStreamIndex;
        bootstrap.sourceWidth = videoStream->codecpar->width;
        bootstrap.sourceHeight = videoStream->codecpar->height;
        bootstrap.sourcePixelFormat = static_cast<AVPixelFormat>(videoStream->codecpar->format);
        bootstrap.frameRate = frameRate;
        bootstrap.timeBase = videoStream->time_base.num > 0 && videoStream->time_base.den > 0 ? videoStream->time_base : AVRational{1, 30};
        bootstrap.decoderName = decoder->name ? decoder->name : "unknown";
        bootstrap.encoderName = encoder->name ? encoder->name : "unknown";
        return ::media::Result<VideoCodecBootstrap>::success(std::move(bootstrap));
    }

    return ::media::Result<VideoCodecBootstrap>::failure(
        ::media::ErrorInfo::unsupported("failed to create video encoder: " + lastEncoderError));
}

::media::Status pushCodecContext(MediaGraphRuntime& runtime,
                                 MediaNodeId nodeId,
                                 const std::string& portName,
                                 ::media::ffmpeg::CodecContextPtr codecContext)
{
    MediaChannel* channel = runtime.context().findInputChannel(nodeId, portName);
    if (!channel) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("codec input channel not found: " + portName));
    }

    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(codecContext));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    return channel->push(std::move(buffer).value());
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5 || hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h")) {
        printUsage();
        return argc < 5 ? 2 : 0;
    }

    LocalFileTranscodeOptions options;
    options.inputUrl = getArg(argc, argv, "--input");
    options.outputUrl = getArg(argc, argv, "--output");
    options.outputFormat = getArg(argc, argv, "--format");

    options.includeVideo = !hasArg(argc, argv, "--no-video");
    options.includeAudio = !hasArg(argc, argv, "--no-audio");
    options.audioTranscode = hasArg(argc, argv, "--audio-transcode");
    options.disableHardware = hasArg(argc, argv, "--disable-hw");
    options.useHardwareTransfer = !options.disableHardware;

    options.videoCodec = getArg(argc, argv, "--video-codec", options.videoCodec);
    options.videoEncoder = getArg(argc, argv, "--encoder", options.videoEncoder);
    options.rateControlMode = getArg(argc, argv, "--rc", options.rateControlMode);
    options.speedPreset = getArg(argc, argv, "--preset", options.speedPreset);
    options.profile = getArg(argc, argv, "--profile", options.profile);
    options.tune = getArg(argc, argv, "--tune", options.tune);
    options.level = getArg(argc, argv, "--level", options.level);

    options.width = parseInt(argc, argv, "--width", options.width);
    options.height = parseInt(argc, argv, "--height", options.height);
    options.fpsNum = parseInt(argc, argv, "--fps", options.fpsNum);
    options.videoBitrateKbps = parseInt(argc, argv, "--bitrate", options.videoBitrateKbps);
    options.crf = parseInt(argc, argv, "--crf", options.crf);
    options.quality = parseInt(argc, argv, "--quality", options.quality);
    options.gop = parseInt(argc, argv, "--gop", options.gop);
    options.maxBFrames = parseInt(argc, argv, "--bframes", options.maxBFrames);

    options.audioCodec = getArg(argc, argv, "--audio-codec", options.audioCodec);
    options.audioBitrateKbps = parseInt(argc, argv, "--audio-bitrate", options.audioBitrateKbps);
    options.audioSampleRate = parseInt(argc, argv, "--sample-rate", options.audioSampleRate);
    options.audioChannels = parseInt(argc, argv, "--channels", options.audioChannels);

    const std::size_t maxIterations = static_cast<std::size_t>(parseInt(argc, argv, "--max-iterations", 1024));
    const std::size_t idleThreshold = static_cast<std::size_t>(parseInt(argc, argv, "--idle-threshold", 16));

    std::cout << "[CLI] input=" << options.inputUrl
              << " output=" << options.outputUrl
              << " video=" << (options.includeVideo ? "on" : "off")
              << " audio=" << (options.includeAudio ? "on" : "off")
              << " width=" << options.width
              << " height=" << options.height
              << " fps=" << options.fpsNum
              << " bitrate_kbps=" << options.videoBitrateKbps
              << " rc=" << options.rateControlMode
              << '\n';

    auto graphResult = LocalFileTranscodeGraphBuilder::build(options);
    if (!graphResult) {
        return failResult("graph build", graphResult);
    }

    MediaGraph graph = std::move(graphResult).value();
    const MediaNodeId videoDecodeNode = findNodeByName(graph, "local.video.decode");
    const MediaNodeId videoEncodeNode = findNodeByName(graph, "local.video.encode");

    auto bootstrap = options.includeVideo ? createVideoCodecBootstrap(options)
                                          : ::media::Result<VideoCodecBootstrap>::success({});
    if (!bootstrap) {
        return failResult("codec bootstrap", bootstrap);
    }

    MediaGraphRuntime runtime;
    auto compileStatus = runtime.compile(std::move(graph));
    if (!compileStatus) {
        return failStatus("compile", compileStatus);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }

    if (options.includeVideo) {
        if (!videoDecodeNode.isValid() || !videoEncodeNode.isValid()) {
            std::cerr << "[CLI] video decode/encode node not found\n";
            return 1;
        }

        std::cout << "[CLI] codec bootstrap: decoder=" << bootstrap.value().decoderName
                  << " encoder=" << bootstrap.value().encoderName
                  << " source=" << bootstrap.value().sourceWidth << 'x' << bootstrap.value().sourceHeight
                  << " stream_index=" << bootstrap.value().videoStreamIndex
                  << '\n';

        auto pushDecoder = pushCodecContext(runtime,
                                            videoDecodeNode,
                                            "codec",
                                            std::move(bootstrap.value().decoderContext));
        if (!pushDecoder) {
            return failStatus("push decoder codec", pushDecoder);
        }

        auto pushEncoder = pushCodecContext(runtime,
                                            videoEncodeNode,
                                            "codec",
                                            std::move(bootstrap.value().encoderContext));
        if (!pushEncoder) {
            return failStatus("push encoder codec", pushEncoder);
        }
    }

    auto startStatus = runtime.start();
    if (!startStatus) {
        return failStatus("start", startStatus);
    }

    MediaGraphRunLoopOptions runOptions;
    runOptions.maxIterations = maxIterations;
    runOptions.idleThreshold = idleThreshold;
    runOptions.stopOnIdle = true;

    auto runResult = runtime.runUntilIdle(runOptions);
    auto stopStatus = runtime.stop();
    if (!stopStatus) {
        return failStatus("stop", stopStatus);
    }
    if (!runResult) {
        return failResult("runUntilIdle", runResult);
    }

    const auto& result = runResult.value();
    std::cout << "[CLI] done: iterations=" << result.iterations
              << " idle_iterations=" << result.idleIterations
              << " total_pushed=" << result.totalPushed
              << " total_popped=" << result.totalPopped
              << " queued_buffers=" << result.queuedBuffers
              << " stopped_idle=" << (result.stoppedBecauseIdle ? "true" : "false")
              << " stopped_max_iterations=" << (result.stoppedBecauseMaxIterations ? "true" : "false")
              << '\n';

    return 0;
}
