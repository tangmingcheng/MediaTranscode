#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"

#include "internal/FFmpegRAII.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
}

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

struct HardwareCapability {
    bool available = false;
    std::string reason;
};

using HardwareCapabilityCache = std::unordered_map<std::string, HardwareCapability>;

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string canonicalCodecName(std::string codec)
{
    codec = lowerCopy(std::move(codec));
    if (codec == "avc" || codec == "h.264") {
        return "h264";
    }
    if (codec == "h265" || codec == "h.265") {
        return "hevc";
    }
    return codec;
}

std::string ffmpegErrorString(int errorCode)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, text, sizeof(text));
    return text;
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), value.begin());
}

std::string codecSpecificName(const std::string& codec, const std::string& suffix)
{
    return codec + suffix;
}

std::string softwareEncoderName(const std::string& outputCodec)
{
    if (outputCodec == "h264") {
        return "libx264";
    }
    if (outputCodec == "hevc") {
        return "libx265";
    }
    return outputCodec;
}

bool decoderExists(const std::string& name)
{
    return !name.empty() && avcodec_find_decoder_by_name(name.c_str()) != nullptr;
}

bool encoderExists(const std::string& name)
{
    return !name.empty() && avcodec_find_encoder_by_name(name.c_str()) != nullptr;
}

bool filterExists(const std::string& name)
{
    if (name.empty() || startsWith(name, "passthrough")) {
        return true;
    }

    return avfilter_get_by_name(name.c_str()) != nullptr;
}

bool rkmppRuntimeAvailable()
{
    return decoderExists("h264_rkmpp") || decoderExists("hevc_rkmpp") ||
           encoderExists("h264_rkmpp") || encoderExists("hevc_rkmpp");
}

HardwareCapability probeHardwareCapability(MediaHardwareDeviceKind kind,
                                           const std::string& hwaccelName)
{
    HardwareCapability capability;

    if (kind == MediaHardwareDeviceKind::None) {
        capability.available = true;
        capability.reason = "software path";
        return capability;
    }

    if (kind == MediaHardwareDeviceKind::RKMPP) {
        capability.available = rkmppRuntimeAvailable();
        capability.reason = capability.available ? "rkmpp codec found" : "rkmpp codec not found";
        return capability;
    }

    if (hwaccelName.empty()) {
        capability.available = false;
        capability.reason = "missing hwaccel name";
        return capability;
    }

    const AVHWDeviceType type = av_hwdevice_find_type_by_name(hwaccelName.c_str());
    capability.available = type != AV_HWDEVICE_TYPE_NONE;
    capability.reason = capability.available
                            ? "hw device type found; runtime device not created"
                            : "hw device type not compiled: " + hwaccelName;
    return capability;
}

std::string hardwareCapabilityKey(MediaHardwareDeviceKind kind,
                                  const std::string& hwaccelName)
{
    return std::string(mediaHardwareDeviceKindName(kind)) + ":" + hwaccelName;
}

const HardwareCapability& cachedHardwareCapability(HardwareCapabilityCache& cache,
                                                   MediaHardwareDeviceKind kind,
                                                   const std::string& hwaccelName)
{
    const std::string key = hardwareCapabilityKey(kind, hwaccelName);
    auto iter = cache.find(key);
    if (iter != cache.end()) {
        return iter->second;
    }

    auto inserted = cache.emplace(key, probeHardwareCapability(kind, hwaccelName));
    return inserted.first->second;
}

void applyHardwareCapability(MediaPipelineStagePlan& stage,
                             HardwareCapabilityCache& cache)
{
    if (!stage.available) {
        return;
    }

    if (!stage.hardware) {
        stage.availabilityReason = "available";
        return;
    }

    const HardwareCapability& capability = cachedHardwareCapability(cache, stage.deviceKind, stage.hwaccelName);
    stage.available = capability.available;
    stage.availabilityReason = capability.reason;
}

MediaPipelineStagePlan makeCodecStage(MediaPipelineStageRole role,
                                       std::string componentName,
                                       std::string codecName,
                                       std::string ffmpegName,
                                       std::string hwaccelName,
                                       MediaHardwareDeviceKind deviceKind,
                                       bool hardware,
                                       bool zeroCopy,
                                       int priority)
{
    MediaPipelineStagePlan stage;
    stage.role = role;
    stage.componentName = std::move(componentName);
    stage.codecName = std::move(codecName);
    stage.ffmpegName = std::move(ffmpegName);
    stage.hwaccelName = std::move(hwaccelName);
    stage.deviceKind = deviceKind;
    stage.frameKind = hardware ? MediaHardwareFrameKind::Hardware : MediaHardwareFrameKind::Software;
    stage.hardware = hardware;
    stage.zeroCopy = zeroCopy;
    stage.score = priority;

    const bool codecOk = role == MediaPipelineStageRole::Decoder
                             ? decoderExists(stage.ffmpegName)
                             : encoderExists(stage.ffmpegName);
    stage.available = codecOk;
    stage.availabilityReason = codecOk
                                   ? "codec found"
                                   : std::string(role == MediaPipelineStageRole::Decoder ? "decoder not found: " : "encoder not found: ") + stage.ffmpegName;
    return stage;
}

MediaPipelineStagePlan makeFilterStage(std::string componentName,
                                        std::string filterName,
                                        std::string hwaccelName,
                                        MediaHardwareDeviceKind deviceKind,
                                        bool hardware,
                                        bool zeroCopy,
                                        int priority)
{
    MediaPipelineStagePlan stage;
    stage.role = MediaPipelineStageRole::Filter;
    stage.componentName = std::move(componentName);
    stage.filterName = std::move(filterName);
    stage.hwaccelName = std::move(hwaccelName);
    stage.deviceKind = deviceKind;
    stage.frameKind = hardware ? MediaHardwareFrameKind::Hardware : MediaHardwareFrameKind::Software;
    stage.hardware = hardware;
    stage.zeroCopy = zeroCopy;
    stage.score = priority;

    const bool filterOk = filterExists(stage.filterName);
    stage.available = filterOk;
    stage.availabilityReason = filterOk ? "filter found" : "filter not found: " + stage.filterName;
    return stage;
}

MediaPipelineChainPlan makeRawChain(std::string label,
                                    MediaPipelineStagePlan decoder,
                                    MediaPipelineStagePlan filter,
                                    MediaPipelineStagePlan encoder,
                                    HardwareCapabilityCache& hardwareCache)
{
    MediaPipelineChainPlan chain;
    chain.label = std::move(label);
    chain.decoder = std::move(decoder);
    chain.filter = std::move(filter);
    chain.encoder = std::move(encoder);

    if (!chain.decoder.available || !chain.filter.available || !chain.encoder.available) {
        return chain;
    }

    applyHardwareCapability(chain.decoder, hardwareCache);
    applyHardwareCapability(chain.filter, hardwareCache);
    applyHardwareCapability(chain.encoder, hardwareCache);
    return chain;
}

} // namespace

::media::Result<std::string> MediaPipelineCapabilityScanner::detectInputVideoCodecName(
    const std::string& inputPath)
{
    AVFormatContext* raw = nullptr;
    const int openRet = avformat_open_input(&raw, inputPath.c_str(), nullptr, nullptr);
    if (openRet < 0) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_open_input: " + ffmpegErrorString(openRet), openRet));
    }

    ::media::ffmpeg::InputFormatContextPtr inputContext(raw);
    const int infoRet = avformat_find_stream_info(inputContext.get(), nullptr);
    if (infoRet < 0) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(infoRet), infoRet));
    }

    const int streamIndex = av_find_best_stream(inputContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ffmpegFailure("av_find_best_stream(video): " + ffmpegErrorString(streamIndex), streamIndex));
    }

    const AVCodecParameters* params = inputContext->streams[streamIndex]->codecpar;
    const char* codecName = avcodec_get_name(params->codec_id);
    if (!codecName || std::string(codecName) == "unknown") {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::unsupported("input video codec is unknown"));
    }

    return ::media::Result<std::string>::success(canonicalCodecName(codecName));
}

std::vector<MediaPipelineChainPlan> MediaPipelineCapabilityScanner::enumerateVideoTranscodeCandidates(
    const std::string& inputCodecName,
    const std::string& outputCodecName,
    const MediaPipelinePlannerOptions& options)
{
    const std::string inputCodec = canonicalCodecName(inputCodecName);
    const std::string outputCodec = canonicalCodecName(outputCodecName);
    HardwareCapabilityCache hardwareCache;
    std::vector<MediaPipelineChainPlan> chains;

    auto add = [&](std::string label,
                   MediaPipelineStagePlan decoder,
                   MediaPipelineStagePlan filter,
                   MediaPipelineStagePlan encoder) {
        chains.push_back(makeRawChain(std::move(label), std::move(decoder), std::move(filter), std::move(encoder), hardwareCache));
    };

    add("cuda-nvenc",
        makeCodecStage(MediaPipelineStageRole::Decoder, "cuda decoder", inputCodec,
                       codecSpecificName(inputCodec, "_cuvid"), "cuda", MediaHardwareDeviceKind::CUDA,
                       true, true, 95),
        makeFilterStage("cuda passthrough filter", "passthrough_cuda", "cuda",
                        MediaHardwareDeviceKind::CUDA, true, true, 90),
        makeCodecStage(MediaPipelineStageRole::Encoder, "nvenc encoder", outputCodec,
                       codecSpecificName(outputCodec, "_nvenc"), "cuda", MediaHardwareDeviceKind::CUDA,
                       true, true, 95));

    add("qsv",
        makeCodecStage(MediaPipelineStageRole::Decoder, "qsv decoder", inputCodec,
                       codecSpecificName(inputCodec, "_qsv"), "qsv", MediaHardwareDeviceKind::QSV,
                       true, true, 90),
        makeFilterStage("qsv passthrough filter", "passthrough_qsv", "qsv",
                        MediaHardwareDeviceKind::QSV, true, true, 88),
        makeCodecStage(MediaPipelineStageRole::Encoder, "qsv encoder", outputCodec,
                       codecSpecificName(outputCodec, "_qsv"), "qsv", MediaHardwareDeviceKind::QSV,
                       true, true, 90));

    add("d3d11va-mediafoundation",
        makeCodecStage(MediaPipelineStageRole::Decoder, "d3d11va decoder", inputCodec, inputCodec,
                       "d3d11va", MediaHardwareDeviceKind::D3D11VA, true, true, 84),
        makeFilterStage("d3d11va passthrough filter", "passthrough_d3d11va", "d3d11va",
                        MediaHardwareDeviceKind::D3D11VA, true, true, 82),
        makeCodecStage(MediaPipelineStageRole::Encoder, "mediafoundation encoder", outputCodec,
                       codecSpecificName(outputCodec, "_mf"), "d3d11va", MediaHardwareDeviceKind::D3D11VA,
                       true, true, 84));

    add("rkmpp",
        makeCodecStage(MediaPipelineStageRole::Decoder, "rkmpp decoder", inputCodec,
                       codecSpecificName(inputCodec, "_rkmpp"), "rkmpp", MediaHardwareDeviceKind::RKMPP,
                       true, true, 92),
        makeFilterStage("rga/rkmpp passthrough filter", "passthrough_rkmpp", "rkmpp",
                        MediaHardwareDeviceKind::RKMPP, true, true, 90),
        makeCodecStage(MediaPipelineStageRole::Encoder, "rkmpp encoder", outputCodec,
                       codecSpecificName(outputCodec, "_rkmpp"), "rkmpp", MediaHardwareDeviceKind::RKMPP,
                       true, true, 92));

    add("vaapi",
        makeCodecStage(MediaPipelineStageRole::Decoder, "vaapi decoder", inputCodec, inputCodec,
                       "vaapi", MediaHardwareDeviceKind::VAAPI, true, true, 82),
        makeFilterStage("vaapi passthrough filter", "passthrough_vaapi", "vaapi",
                        MediaHardwareDeviceKind::VAAPI, true, true, 82),
        makeCodecStage(MediaPipelineStageRole::Encoder, "vaapi encoder", outputCodec,
                       codecSpecificName(outputCodec, "_vaapi"), "vaapi", MediaHardwareDeviceKind::VAAPI,
                       true, true, 82));

    add("videotoolbox",
        makeCodecStage(MediaPipelineStageRole::Decoder, "videotoolbox decoder", inputCodec, inputCodec,
                       "videotoolbox", MediaHardwareDeviceKind::VideoToolbox, true, true, 80),
        makeFilterStage("videotoolbox passthrough filter", "passthrough_videotoolbox", "videotoolbox",
                        MediaHardwareDeviceKind::VideoToolbox, true, true, 78),
        makeCodecStage(MediaPipelineStageRole::Encoder, "videotoolbox encoder", outputCodec,
                       codecSpecificName(outputCodec, "_videotoolbox"), "videotoolbox",
                       MediaHardwareDeviceKind::VideoToolbox, true, true, 80));

    if (options.allowSoftwareFallback) {
        add("software",
            makeCodecStage(MediaPipelineStageRole::Decoder, "software decoder", inputCodec, inputCodec,
                           "", MediaHardwareDeviceKind::None, false, false, 30),
            makeFilterStage("software passthrough filter", "passthrough_software", "",
                            MediaHardwareDeviceKind::None, false, false, 30),
            makeCodecStage(MediaPipelineStageRole::Encoder, "software encoder", outputCodec,
                           softwareEncoderName(outputCodec), "", MediaHardwareDeviceKind::None, false, false, 30));

        add("software-native-codec",
            makeCodecStage(MediaPipelineStageRole::Decoder, "software decoder", inputCodec, inputCodec,
                           "", MediaHardwareDeviceKind::None, false, false, 20),
            makeFilterStage("software passthrough filter", "passthrough_software", "",
                            MediaHardwareDeviceKind::None, false, false, 20),
            makeCodecStage(MediaPipelineStageRole::Encoder, "native software encoder", outputCodec, outputCodec,
                           "", MediaHardwareDeviceKind::None, false, false, 20));
    }

    return chains;
}

} // namespace media::ffmpeg::graph
