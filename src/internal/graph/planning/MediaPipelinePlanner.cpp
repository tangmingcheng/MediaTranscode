#include "internal/graph/planning/MediaPipelinePlanner.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

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

bool codecAvailable(MediaPipelineStageRole role, const std::string& ffmpegName)
{
    if (ffmpegName.empty()) {
        return false;
    }

    switch (role) {
    case MediaPipelineStageRole::Decoder:
        return avcodec_find_decoder_by_name(ffmpegName.c_str()) != nullptr;
    case MediaPipelineStageRole::Encoder:
        return avcodec_find_encoder_by_name(ffmpegName.c_str()) != nullptr;
    case MediaPipelineStageRole::Filter:
        return true;
    }

    return false;
}

bool filterAvailable(const std::string& filterName)
{
    if (filterName.empty() || startsWith(filterName, "passthrough")) {
        return true;
    }

    return avfilter_get_by_name(filterName.c_str()) != nullptr;
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

std::string codecSpecificName(const std::string& codec, const std::string& suffix)
{
    return codec + suffix;
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
    stage.available = codecAvailable(role, stage.ffmpegName);
    stage.score = priority;
    stage.availabilityReason = stage.available ? "available" : "ffmpeg component not found";
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
    stage.available = filterAvailable(stage.filterName);
    stage.score = priority;
    stage.availabilityReason = stage.available ? "available" : "ffmpeg filter not found";
    return stage;
}

int stageScore(const MediaPipelineStagePlan& stage, const MediaPipelinePlannerOptions& options)
{
    int score = stage.score;
    score += stage.available ? 120 : -5000;
    score += stage.hardware ? 220 : 40;
    score += stage.zeroCopy ? 60 : 0;

    if (!options.preferGpu && !stage.hardware) {
        score += 80;
    }

    return score;
}

bool hasSameHardwareDevice(const MediaPipelineChainPlan& chain)
{
    if (!chain.allHardware) {
        return false;
    }

    return chain.decoder.deviceKind == chain.filter.deviceKind &&
           chain.filter.deviceKind == chain.encoder.deviceKind &&
           chain.decoder.deviceKind != MediaHardwareDeviceKind::None &&
           chain.decoder.deviceKind != MediaHardwareDeviceKind::Unknown;
}

std::string chainReason(const MediaPipelineChainPlan& chain)
{
    std::ostringstream out;
    if (chain.allHardware && chain.sameHardwareDevice && chain.zeroCopy) {
        out << "full hardware zero-copy chain";
    } else if (chain.allHardware) {
        out << "full hardware chain with transfer risk";
    } else if (chain.decoder.hardware || chain.filter.hardware || chain.encoder.hardware) {
        out << "mixed hardware/software chain";
    } else {
        out << "software fallback chain";
    }

    if (!chain.available) {
        out << "; unavailable component present";
    }
    return out.str();
}

MediaPipelineChainPlan makeChain(std::string label,
                                 MediaPipelineStagePlan decoder,
                                 MediaPipelineStagePlan filter,
                                 MediaPipelineStagePlan encoder,
                                 const MediaPipelinePlannerOptions& options)
{
    MediaPipelineChainPlan chain;
    chain.label = std::move(label);
    chain.decoder = std::move(decoder);
    chain.filter = std::move(filter);
    chain.encoder = std::move(encoder);
    chain.available = chain.decoder.available && chain.filter.available && chain.encoder.available;
    chain.allHardware = chain.decoder.hardware && chain.filter.hardware && chain.encoder.hardware;
    chain.sameHardwareDevice = hasSameHardwareDevice(chain);
    chain.zeroCopy = chain.decoder.zeroCopy && chain.filter.zeroCopy && chain.encoder.zeroCopy && chain.sameHardwareDevice;

    chain.score = stageScore(chain.decoder, options) +
                  stageScore(chain.filter, options) +
                  stageScore(chain.encoder, options);

    if (chain.allHardware) {
        chain.score += options.preferGpu ? 900 : 250;
    }
    if (chain.sameHardwareDevice) {
        chain.score += 450;
    }
    if (chain.zeroCopy) {
        chain.score += 350;
    }
    if ((chain.decoder.hardware || chain.encoder.hardware) && !chain.sameHardwareDevice) {
        chain.score -= 180;
    }
    if (options.requireRuntimeAvailability && !chain.available) {
        chain.score -= 100000;
    }

    const std::string preferred = lowerCopy(options.preferredHardware);
    if (!preferred.empty() && preferred != "auto") {
        const std::string selectedDevice = lowerCopy(mediaHardwareDeviceKindName(chain.decoder.deviceKind));
        if (selectedDevice == preferred && chain.sameHardwareDevice) {
            chain.score += 500;
        } else if (chain.allHardware) {
            chain.score -= 100;
        }
    }

    chain.reason = chainReason(chain);
    return chain;
}

std::vector<MediaPipelineChainPlan> candidateChains(const std::string& inputCodec,
                                                    const std::string& outputCodec,
                                                    const MediaPipelinePlannerOptions& options)
{
    std::vector<MediaPipelineChainPlan> chains;

    auto add = [&](std::string label,
                   MediaPipelineStagePlan decoder,
                   MediaPipelineStagePlan filter,
                   MediaPipelineStagePlan encoder) {
        chains.push_back(makeChain(std::move(label), std::move(decoder), std::move(filter), std::move(encoder), options));
    };

    add("cuda-nvenc",
        makeCodecStage(MediaPipelineStageRole::Decoder,
                       "cuda decoder",
                       inputCodec,
                       codecSpecificName(inputCodec, "_cuvid"),
                       "cuda",
                       MediaHardwareDeviceKind::CUDA,
                       true,
                       true,
                       95),
        makeFilterStage("cuda passthrough filter",
                        "passthrough_cuda",
                        "cuda",
                        MediaHardwareDeviceKind::CUDA,
                        true,
                        true,
                        90),
        makeCodecStage(MediaPipelineStageRole::Encoder,
                       "nvenc encoder",
                       outputCodec,
                       codecSpecificName(outputCodec, "_nvenc"),
                       "cuda",
                       MediaHardwareDeviceKind::CUDA,
                       true,
                       true,
                       95));

    add("qsv",
        makeCodecStage(MediaPipelineStageRole::Decoder,
                       "qsv decoder",
                       inputCodec,
                       codecSpecificName(inputCodec, "_qsv"),
                       "qsv",
                       MediaHardwareDeviceKind::D3D11VA,
                       true,
                       true,
                       90),
        makeFilterStage("qsv passthrough filter",
                        "passthrough_qsv",
                        "qsv",
                        MediaHardwareDeviceKind::D3D11VA,
                        true,
                        true,
                        88),
        makeCodecStage(MediaPipelineStageRole::Encoder,
                       "qsv encoder",
                       outputCodec,
                       codecSpecificName(outputCodec, "_qsv"),
                       "qsv",
                       MediaHardwareDeviceKind::D3D11VA,
                       true,
                       true,
                       90));

    add("d3d11va-mediafoundation",
        makeCodecStage(MediaPipelineStageRole::Decoder,
                       "d3d11va decoder",
                       inputCodec,
                       inputCodec,
                       "d3d11va",
                       MediaHardwareDeviceKind::D3D11VA,
                       true,
                       true,
                       84),
        makeFilterStage("d3d11va passthrough filter",
                        "passthrough_d3d11va",
                        "d3d11va",
                        MediaHardwareDeviceKind::D3D11VA,
                        true,
                        true,
                        82),
        makeCodecStage(MediaPipelineStageRole::Encoder,
                       "mediafoundation encoder",
                       outputCodec,
                       codecSpecificName(outputCodec, "_mf"),
                       "d3d11va",
                       MediaHardwareDeviceKind::D3D11VA,
                       true,
                       true,
                       84));

    add("rkmpp",
        makeCodecStage(MediaPipelineStageRole::Decoder,
                       "rkmpp decoder",
                       inputCodec,
                       codecSpecificName(inputCodec, "_rkmpp"),
                       "rkmpp",
                       MediaHardwareDeviceKind::RKMPP,
                       true,
                       true,
                       92),
        makeFilterStage("rga/rkmpp passthrough filter",
                        "passthrough_rkmpp",
                        "rkmpp",
                        MediaHardwareDeviceKind::RKMPP,
                        true,
                        true,
                        90),
        makeCodecStage(MediaPipelineStageRole::Encoder,
                       "rkmpp encoder",
                       outputCodec,
                       codecSpecificName(outputCodec, "_rkmpp"),
                       "rkmpp",
                       MediaHardwareDeviceKind::RKMPP,
                       true,
                       true,
                       92));

    add("vaapi",
        makeCodecStage(MediaPipelineStageRole::Decoder,
                       "vaapi decoder",
                       inputCodec,
                       inputCodec,
                       "vaapi",
                       MediaHardwareDeviceKind::VAAPI,
                       true,
                       true,
                       82),
        makeFilterStage("vaapi passthrough filter",
                        "passthrough_vaapi",
                        "vaapi",
                        MediaHardwareDeviceKind::VAAPI,
                        true,
                        true,
                        82),
        makeCodecStage(MediaPipelineStageRole::Encoder,
                       "vaapi encoder",
                       outputCodec,
                       codecSpecificName(outputCodec, "_vaapi"),
                       "vaapi",
                       MediaHardwareDeviceKind::VAAPI,
                       true,
                       true,
                       82));

    add("videotoolbox",
        makeCodecStage(MediaPipelineStageRole::Decoder,
                       "videotoolbox decoder",
                       inputCodec,
                       inputCodec,
                       "videotoolbox",
                       MediaHardwareDeviceKind::VideoToolbox,
                       true,
                       true,
                       80),
        makeFilterStage("videotoolbox passthrough filter",
                        "passthrough_videotoolbox",
                        "videotoolbox",
                        MediaHardwareDeviceKind::VideoToolbox,
                        true,
                        true,
                        78),
        makeCodecStage(MediaPipelineStageRole::Encoder,
                       "videotoolbox encoder",
                       outputCodec,
                       codecSpecificName(outputCodec, "_videotoolbox"),
                       "videotoolbox",
                       MediaHardwareDeviceKind::VideoToolbox,
                       true,
                       true,
                       80));

    if (options.allowSoftwareFallback) {
        add("software",
            makeCodecStage(MediaPipelineStageRole::Decoder,
                           "software decoder",
                           inputCodec,
                           inputCodec,
                           "",
                           MediaHardwareDeviceKind::None,
                           false,
                           false,
                           30),
            makeFilterStage("software passthrough filter",
                            "passthrough_software",
                            "",
                            MediaHardwareDeviceKind::None,
                            false,
                            false,
                            30),
            makeCodecStage(MediaPipelineStageRole::Encoder,
                           "software encoder",
                           outputCodec,
                           softwareEncoderName(outputCodec),
                           "",
                           MediaHardwareDeviceKind::None,
                           false,
                           false,
                           30));

        add("software-native-codec",
            makeCodecStage(MediaPipelineStageRole::Decoder,
                           "software decoder",
                           inputCodec,
                           inputCodec,
                           "",
                           MediaHardwareDeviceKind::None,
                           false,
                           false,
                           20),
            makeFilterStage("software passthrough filter",
                            "passthrough_software",
                            "",
                            MediaHardwareDeviceKind::None,
                            false,
                            false,
                            20),
            makeCodecStage(MediaPipelineStageRole::Encoder,
                           "native software encoder",
                           outputCodec,
                           outputCodec,
                           "",
                           MediaHardwareDeviceKind::None,
                           false,
                           false,
                           20));
    }

    std::sort(chains.begin(), chains.end(), [](const MediaPipelineChainPlan& lhs,
                                               const MediaPipelineChainPlan& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.label < rhs.label;
    });

    return chains;
}

::media::Result<std::string> detectInputVideoCodecName(const std::string& inputPath)
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

bool setOption(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    return graph.setNodeOption(nodeId, key, value);
}

::media::Status applyStageOptions(MediaGraph& graph,
                                  MediaNodeId nodeId,
                                  const MediaPipelineStagePlan& stage,
                                  const MediaPipelineChainPlan& chain)
{
    bool ok = true;
    ok = ok && setOption(graph, nodeId, "pipeline.chain", chain.label);
    ok = ok && setOption(graph, nodeId, "pipeline.chain_score", std::to_string(chain.score));
    ok = ok && setOption(graph, nodeId, "pipeline.stage", mediaPipelineStageRoleName(stage.role));
    ok = ok && setOption(graph, nodeId, "pipeline.component", stage.componentName);
    ok = ok && setOption(graph, nodeId, "pipeline.available", stage.available ? "1" : "0");
    ok = ok && setOption(graph, nodeId, "pipeline.zero_copy", stage.zeroCopy ? "1" : "0");
    ok = ok && setOption(graph, nodeId, "pipeline.hardware", stage.hardware ? "1" : "0");
    ok = ok && setOption(graph, nodeId, "pipeline.hwaccel", stage.hwaccelName);
    ok = ok && setOption(graph, nodeId, "pipeline.device", mediaHardwareDeviceKindName(stage.deviceKind));
    ok = ok && setOption(graph, nodeId, "pipeline.frame_kind", mediaHardwareFrameKindName(stage.frameKind));

    if (!stage.codecName.empty()) {
        ok = ok && setOption(graph, nodeId, "codec", stage.codecName);
    }
    if (!stage.ffmpegName.empty()) {
        const char* key = stage.role == MediaPipelineStageRole::Decoder ? "decoder" : "encoder";
        ok = ok && setOption(graph, nodeId, key, stage.ffmpegName);
    }
    if (!stage.filterName.empty()) {
        ok = ok && setOption(graph, nodeId, "filter", stage.filterName);
    }

    if (!ok) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("failed to apply media pipeline stage options"));
    }

    return ::media::Status::success();
}

MediaEdgePolicy queuePolicy(std::size_t capacity = 256)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    return policy;
}

} // namespace

const char* mediaPipelineStageRoleName(MediaPipelineStageRole role) noexcept
{
    switch (role) {
    case MediaPipelineStageRole::Decoder:
        return "decoder";
    case MediaPipelineStageRole::Filter:
        return "filter";
    case MediaPipelineStageRole::Encoder:
        return "encoder";
    }
    return "unknown";
}

const char* mediaHardwareDeviceKindName(MediaHardwareDeviceKind kind) noexcept
{
    switch (kind) {
    case MediaHardwareDeviceKind::Unknown:
        return "unknown";
    case MediaHardwareDeviceKind::None:
        return "software";
    case MediaHardwareDeviceKind::D3D11VA:
        return "d3d11va";
    case MediaHardwareDeviceKind::CUDA:
        return "cuda";
    case MediaHardwareDeviceKind::VAAPI:
        return "vaapi";
    case MediaHardwareDeviceKind::DRMPrime:
        return "drm_prime";
    case MediaHardwareDeviceKind::RKMPP:
        return "rkmpp";
    case MediaHardwareDeviceKind::VideoToolbox:
        return "videotoolbox";
    case MediaHardwareDeviceKind::MediaCodec:
        return "mediacodec";
    }
    return "unknown";
}

const char* mediaHardwareFrameKindName(MediaHardwareFrameKind kind) noexcept
{
    switch (kind) {
    case MediaHardwareFrameKind::Unknown:
        return "unknown";
    case MediaHardwareFrameKind::Software:
        return "software";
    case MediaHardwareFrameKind::Hardware:
        return "hardware";
    case MediaHardwareFrameKind::HardwareMapped:
        return "hardware_mapped";
    }
    return "unknown";
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoTranscodeFile(
    const std::string& inputPath,
    MediaPipelinePlannerOptions options)
{
    if (inputPath.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("planVideoTranscodeFile requires input path"));
    }

    auto inputCodec = detectInputVideoCodecName(inputPath);
    if (!inputCodec) {
        return ::media::Result<MediaPipelinePlan>::failure(inputCodec.error());
    }

    MediaPipelinePlan plan;
    plan.inputPath = inputPath;
    plan.outputPath = std::move(options.outputPath);
    plan.inputCodecName = canonicalCodecName(inputCodec.value());
    plan.outputCodecName = canonicalCodecName(options.outputCodecName.empty() ? plan.inputCodecName : options.outputCodecName);
    plan.candidates = candidateChains(plan.inputCodecName, plan.outputCodecName, options);

    if (plan.candidates.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::unsupported("no media pipeline candidates were generated"));
    }

    auto selected = std::find_if(plan.candidates.begin(), plan.candidates.end(), [&](const MediaPipelineChainPlan& chain) {
        return !options.requireRuntimeAvailability || chain.available;
    });

    if (selected == plan.candidates.end()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::hardwareUnavailable("no available decoder/filter/encoder chain found"));
    }

    plan.selected = *selected;
    return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
}

::media::Status MediaPipelinePlanner::applyVideoPlanToGraph(MediaGraph& graph,
                                                            MediaNodeId videoDecodeNode,
                                                            MediaNodeId videoFilterNode,
                                                            MediaNodeId videoEncodeNode,
                                                            const MediaPipelinePlan& plan)
{
    auto decodeStatus = applyStageOptions(graph, videoDecodeNode, plan.selected.decoder, plan.selected);
    if (!decodeStatus) {
        return decodeStatus;
    }

    auto filterStatus = applyStageOptions(graph, videoFilterNode, plan.selected.filter, plan.selected);
    if (!filterStatus) {
        return filterStatus;
    }

    return applyStageOptions(graph, videoEncodeNode, plan.selected.encoder, plan.selected);
}

::media::Result<MediaPipelineGraphBuildResult> MediaPipelinePlanner::buildPlannedVideoFileTranscodeGraph(
    const std::string& inputPath,
    MediaPipelinePlannerOptions options)
{
    auto plan = planVideoTranscodeFile(inputPath, options);
    if (!plan) {
        return ::media::Result<MediaPipelineGraphBuildResult>::failure(plan.error());
    }

    MediaPipelineGraphBuildResult result;
    result.plan = std::move(plan).value();

    MediaGraph graph;
    result.fileInputNode = graph.addNode(MediaNodeKind::FileInput, "file-input");
    result.demuxNode = graph.addNode(MediaNodeKind::Demux, "demux");
    result.streamSplitNode = graph.addNode(MediaNodeKind::StreamSplit, "stream-split");
    result.videoDecodeNode = graph.addNode(MediaNodeKind::VideoDecode, "video-decode");
    result.videoFilterNode = graph.addNode(MediaNodeKind::VideoFilter, "video-filter");
    result.videoEncodeNode = graph.addNode(MediaNodeKind::VideoEncode, "video-encode");
    result.fileOutputNode = graph.addNode(MediaNodeKind::FileOutput, "file-output");
    result.fileMuxNode = graph.addNode(MediaNodeKind::FileMux, "file-mux");

    graph.setNodeOption(result.fileInputNode, "path", result.plan.inputPath);
    graph.setNodeOption(result.fileOutputNode,
                        "path",
                        result.plan.outputPath.empty() ? "planned-output.mp4" : result.plan.outputPath);

    graph.addOutputPort(result.fileInputNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(result.demuxNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext, true, false);

    graph.addOutputPort(result.demuxNode, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet, true, true);
    graph.addInputPort(result.streamSplitNode, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(result.streamSplitNode, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet, true, true);

    graph.addInputPort(result.videoDecodeNode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(result.videoDecodeNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                        MediaPayloadKind::Frame, true, true);

    graph.addInputPort(result.videoFilterNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                       MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(result.videoFilterNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                        MediaPayloadKind::Frame, true, true);

    graph.addInputPort(result.videoEncodeNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                       MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(result.videoEncodeNode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
                        MediaPayloadKind::Packet, true, true);

    graph.addOutputPort(result.fileOutputNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(result.fileMuxNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(result.fileMuxNode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
                       MediaPayloadKind::Packet, true, true);

    graph.connect(result.fileInputNode, "format", result.demuxNode, "format", "file-input-to-demux", queuePolicy(1));
    graph.connect(result.demuxNode, "packet", result.streamSplitNode, "packet", "demux-to-stream-split", queuePolicy(256));
    graph.connect(result.streamSplitNode, "video", result.videoDecodeNode, "packet", "stream-split-to-video-decode", queuePolicy(256));
    graph.connect(result.videoDecodeNode, "frame", result.videoFilterNode, "frame", "video-decode-to-filter", queuePolicy(256));
    graph.connect(result.videoFilterNode, "frame", result.videoEncodeNode, "frame", "video-filter-to-encode", queuePolicy(256));
    graph.connect(result.fileOutputNode, "format", result.fileMuxNode, "format", "file-output-to-mux", queuePolicy(1));
    graph.connect(result.videoEncodeNode, "packet", result.fileMuxNode, "packet", "video-encode-to-mux", queuePolicy(256));

    auto applyStatus = applyVideoPlanToGraph(graph,
                                             result.videoDecodeNode,
                                             result.videoFilterNode,
                                             result.videoEncodeNode,
                                             result.plan);
    if (!applyStatus) {
        return ::media::Result<MediaPipelineGraphBuildResult>::failure(applyStatus.error());
    }

    result.graph = std::move(graph);
    return ::media::Result<MediaPipelineGraphBuildResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
