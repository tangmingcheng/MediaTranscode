#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"

#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaEdgePolicy blockingQueuePolicy(std::size_t capacity)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    policy.queuePolicy.preserveOrdering = true;
    policy.queuePolicy.allowFlushControlBypass = true;
    policy.queuePolicy.collectMetrics = true;
    return policy;
}

void applyVideoOptions(MediaGraph& graph, MediaNodeId nodeId, const LocalFileTranscodeOptions& options)
{
    graph.setNodeOption(nodeId, "video_codec", options.videoCodec);
    graph.setNodeOption(nodeId, "encoder", options.videoEncoder);
    graph.setNodeOption(nodeId, "rc", options.rateControlMode);
    graph.setNodeOption(nodeId, "preset", options.speedPreset);
    graph.setNodeOption(nodeId, "profile", options.profile);
    graph.setNodeOption(nodeId, "tune", options.tune);
    graph.setNodeOption(nodeId, "level", options.level);
    graph.setNodeOption(nodeId, "width", std::to_string(options.width));
    graph.setNodeOption(nodeId, "height", std::to_string(options.height));
    graph.setNodeOption(nodeId, "fps_num", std::to_string(options.fpsNum));
    graph.setNodeOption(nodeId, "fps_den", std::to_string(options.fpsDen));
    graph.setNodeOption(nodeId, "bitrate_kbps", std::to_string(options.videoBitrateKbps));
    graph.setNodeOption(nodeId, "crf", std::to_string(options.crf));
    graph.setNodeOption(nodeId, "quality", std::to_string(options.quality));
    graph.setNodeOption(nodeId, "gop", std::to_string(options.gop));
    graph.setNodeOption(nodeId, "bframes", std::to_string(options.maxBFrames));
}

void preferSoftwarePlan(MediaPipelinePlan& plan)
{
    auto software = std::find_if(plan.candidates.begin(), plan.candidates.end(), [](const MediaPipelineChainPlan& chain) {
        return chain.available && chain.label == "software";
    });
    if (software == plan.candidates.end()) {
        software = std::find_if(plan.candidates.begin(), plan.candidates.end(), [](const MediaPipelineChainPlan& chain) {
            return chain.available && chain.label == "software-native-codec";
        });
    }
    if (software != plan.candidates.end()) {
        plan.selected = *software;
    }
}

::media::Result<MediaPipelinePlan> buildVideoPlan(const LocalFileTranscodeOptions& options)
{
    MediaPipelinePlannerOptions plannerOptions;
    plannerOptions.outputPath = options.outputUrl;
    plannerOptions.outputCodecName = options.videoCodec.empty() ? "h264" : options.videoCodec;
    plannerOptions.allowSoftwareFallback = true;
    plannerOptions.requireRuntimeAvailability = true;
    plannerOptions.preferGpu = options.useHardwareTransfer && !options.disableHardware;
    plannerOptions.preferredHardware = plannerOptions.preferGpu ? "auto" : "software";
    plannerOptions.diagnosticLogEnabled = options.diagnosticLogEnabled;

    auto plan = MediaPipelinePlanner::planVideoTranscodeFile(options.inputUrl, plannerOptions);
    if (!plan) {
        return plan;
    }

    if (!plannerOptions.preferGpu) {
        preferSoftwarePlan(plan.value());
    }

    return plan;
}

} // namespace

::media::Status LocalFileTranscodeGraphBuilder::validate(const LocalFileTranscodeOptions& options)
{
    if (options.inputUrl.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires inputUrl"));
    }

    if (options.outputUrl.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires outputUrl"));
    }

    if (!options.includeVideo && !options.includeAudio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires video or audio branch"));
    }

    if (!options.includeVideo) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("LocalFileTranscodeGraphBuilder audio-only graph requires audio stream config support"));
    }

    if (options.metadataQueueCapacity == 0 ||
        options.packetQueueCapacity == 0 ||
        options.frameQueueCapacity == 0 ||
        options.muxQueueCapacity == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder queue capacities must be greater than 0"));
    }

    return ::media::Status::success();
}

::media::Result<MediaGraph> LocalFileTranscodeGraphBuilder::build(const LocalFileTranscodeOptions& options)
{
    auto validation = validate(options);
    if (!validation) {
        return ::media::Result<MediaGraph>::failure(validation.error());
    }

    std::optional<MediaPipelinePlan> videoPlan;
    if (options.includeVideo) {
        auto plan = buildVideoPlan(options);
        if (!plan) {
            return ::media::Result<MediaGraph>::failure(plan.error());
        }
        videoPlan = std::move(plan).value();
    }

    MediaGraph graph;

    const MediaNodeId fileInput = graph.addNode(
        MediaNodeKind::FileInput,
        "local.file.input",
        "Local file input");
    const MediaNodeId demux = graph.addNode(
        MediaNodeKind::Demux,
        "local.demux",
        "Local demux");
    const MediaNodeId split = graph.addNode(
        MediaNodeKind::StreamSplit,
        "local.stream.split",
        "Local stream split");
    const MediaNodeId fileOutput = graph.addNode(
        MediaNodeKind::FileOutput,
        "local.file.output",
        "Local file output");
    const MediaNodeId mux = graph.addNode(
        MediaNodeKind::FileMux,
        "local.file.mux",
        "Local file mux");

    graph.setNodeOption(fileInput, "url", options.inputUrl);
    graph.setNodeOption(fileOutput, "url", options.outputUrl);
    if (!options.outputFormat.empty()) {
        graph.setNodeOption(fileOutput, "format", options.outputFormat);
    }

    graph.addOutputPort(fileInput,
                        "format",
                        MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext,
                        true,
                        true);
    graph.addInputPort(demux,
                       "format",
                       MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext,
                       true,
                       false);
    graph.addOutputPort(demux,
                        "packet",
                        MediaStreamKind::Any,
                        MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet,
                        true,
                        true);
    graph.addInputPort(split,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.addOutputPort(fileOutput,
                        "format",
                        MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext,
                        true,
                        false);
    graph.addInputPort(mux,
                       "format",
                       MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext,
                       true,
                       false);
    graph.addInputPort(mux,
                       "codec",
                       MediaStreamKind::Video,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::CodecContext,
                       true,
                       true);
    graph.addInputPort(mux,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::Unknown,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.connect(fileInput,
                  "format",
                  demux,
                  "format",
                  "local.file.input.format -> local.demux.format",
                  blockingQueuePolicy(options.metadataQueueCapacity));
    graph.connect(demux,
                  "packet",
                  split,
                  "packet",
                  "local.demux.packet -> local.stream.split.packet",
                  blockingQueuePolicy(options.packetQueueCapacity));
    graph.connect(fileOutput,
                  "format",
                  mux,
                  "format",
                  "local.file.output.format -> local.file.mux.format",
                  blockingQueuePolicy(options.metadataQueueCapacity));

    if (options.includeVideo) {
        const MediaNodeId codecResolver = graph.addNode(
            MediaNodeKind::CodecResolver,
            "local.codec.resolver",
            "Local codec resolver");
        const MediaNodeId videoDecode = graph.addNode(
            MediaNodeKind::VideoDecode,
            "local.video.decode",
            "Local video decode");
        const MediaNodeId videoFilter = graph.addNode(
            MediaNodeKind::VideoFilter,
            "local.video.filter",
            "Local software video filter");
        const MediaNodeId videoTimestamp = graph.addNode(
            MediaNodeKind::VideoTimestamp,
            "local.video.timestamp",
            "Local video timestamp normalize");
        const MediaNodeId videoEncode = graph.addNode(
            MediaNodeKind::VideoEncode,
            "local.video.encode",
            "Local video encode");

        applyVideoOptions(graph, codecResolver, options);
        applyVideoOptions(graph, videoFilter, options);
        applyVideoOptions(graph, videoEncode, options);
        graph.setNodeOption(videoFilter, "pix_fmt", "yuv420p");

        if (videoPlan) {
            auto applyPlanStatus = MediaPipelinePlanner::applyVideoPlanToGraph(
                graph,
                videoDecode,
                videoFilter,
                videoEncode,
                *videoPlan);
            if (!applyPlanStatus) {
                return ::media::Result<MediaGraph>::failure(applyPlanStatus.error());
            }

            graph.setNodeOption(codecResolver, "decoder", videoPlan->selected.decoder.ffmpegName);
            graph.setNodeOption(codecResolver, "encoder", videoPlan->selected.encoder.ffmpegName);
            graph.setNodeOption(codecResolver, "video_codec", videoPlan->outputCodecName);
        }

        if (!options.videoEncoder.empty()) {
            graph.setNodeOption(codecResolver, "encoder", options.videoEncoder);
            graph.setNodeOption(videoEncode, "encoder", options.videoEncoder);
        }

        graph.addInputPort(codecResolver,
                           "format",
                           MediaStreamKind::Metadata,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::FormatContext,
                           true,
                           false);
        graph.addOutputPort(codecResolver,
                            "decoder",
                            MediaStreamKind::Video,
                            MediaEdgeKind::Metadata,
                            MediaPayloadKind::CodecContext,
                            true,
                            false);
        graph.addOutputPort(codecResolver,
                            "timestamp_source",
                            MediaStreamKind::Video,
                            MediaEdgeKind::Metadata,
                            MediaPayloadKind::CodecContext,
                            true,
                            false);
        graph.addOutputPort(codecResolver,
                            "encoder",
                            MediaStreamKind::Video,
                            MediaEdgeKind::Metadata,
                            MediaPayloadKind::CodecContext,
                            true,
                            false);
        graph.addOutputPort(codecResolver,
                            "mux_video",
                            MediaStreamKind::Video,
                            MediaEdgeKind::Metadata,
                            MediaPayloadKind::CodecContext,
                            true,
                            false);

        graph.addInputPort(videoDecode,
                           "codec",
                           MediaStreamKind::Video,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::CodecContext,
                           true,
                           false);
        graph.addOutputPort(split,
                            "video",
                            MediaStreamKind::Video,
                            MediaEdgeKind::InputPacket,
                            MediaPayloadKind::Packet,
                            false,
                            true);
        graph.addInputPort(videoDecode,
                           "packet",
                           MediaStreamKind::Video,
                           MediaEdgeKind::InputPacket,
                           MediaPayloadKind::Packet,
                           true,
                           true);
        graph.addOutputPort(videoDecode,
                            "frame",
                            MediaStreamKind::Video,
                            MediaEdgeKind::RawFrame,
                            MediaPayloadKind::Frame,
                            true,
                            true);

        graph.addInputPort(videoFilter,
                           "frame",
                           MediaStreamKind::Video,
                           MediaEdgeKind::RawFrame,
                           MediaPayloadKind::Frame,
                           true,
                           true);
        graph.addOutputPort(videoFilter,
                            "frame",
                            MediaStreamKind::Video,
                            MediaEdgeKind::RawFrame,
                            MediaPayloadKind::Frame,
                            true,
                            true);

        graph.addInputPort(videoTimestamp,
                           "source_codec",
                           MediaStreamKind::Video,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::CodecContext,
                           true,
                           false);
        graph.addInputPort(videoTimestamp,
                           "target_codec",
                           MediaStreamKind::Video,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::CodecContext,
                           true,
                           false);
        graph.addOutputPort(videoTimestamp,
                            "target_codec",
                            MediaStreamKind::Video,
                            MediaEdgeKind::Metadata,
                            MediaPayloadKind::CodecContext,
                            true,
                            false);
        graph.addInputPort(videoTimestamp,
                           "frame",
                           MediaStreamKind::Video,
                           MediaEdgeKind::RawFrame,
                           MediaPayloadKind::Frame,
                           true,
                           true);
        graph.addOutputPort(videoTimestamp,
                            "frame",
                            MediaStreamKind::Video,
                            MediaEdgeKind::RawFrame,
                            MediaPayloadKind::Frame,
                            true,
                            true);

        graph.addInputPort(videoEncode,
                           "codec",
                           MediaStreamKind::Video,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::CodecContext,
                           true,
                           false);
        graph.addInputPort(videoEncode,
                           "frame",
                           MediaStreamKind::Video,
                           MediaEdgeKind::RawFrame,
                           MediaPayloadKind::Frame,
                           true,
                           true);
        graph.addOutputPort(videoEncode,
                            "packet",
                            MediaStreamKind::Video,
                            MediaEdgeKind::EncodedPacket,
                            MediaPayloadKind::Packet,
                            true,
                            true);

        graph.connect(fileInput,
                      "format",
                      codecResolver,
                      "format",
                      "local.file.input.format -> local.codec.resolver.format",
                      blockingQueuePolicy(options.metadataQueueCapacity));
        graph.connect(codecResolver,
                      "decoder",
                      videoDecode,
                      "codec",
                      "local.codec.resolver.decoder -> local.video.decode.codec",
                      blockingQueuePolicy(options.metadataQueueCapacity));
        graph.connect(codecResolver,
                      "timestamp_source",
                      videoTimestamp,
                      "source_codec",
                      "local.codec.resolver.timestamp_source -> local.video.timestamp.source_codec",
                      blockingQueuePolicy(options.metadataQueueCapacity));
        graph.connect(codecResolver,
                      "encoder",
                      videoTimestamp,
                      "target_codec",
                      "local.codec.resolver.encoder -> local.video.timestamp.target_codec",
                      blockingQueuePolicy(options.metadataQueueCapacity));
        graph.connect(videoTimestamp,
                      "target_codec",
                      videoEncode,
                      "codec",
                      "local.video.timestamp.target_codec -> local.video.encode.codec",
                      blockingQueuePolicy(options.metadataQueueCapacity));
        graph.connect(codecResolver,
                      "mux_video",
                      mux,
                      "codec",
                      "local.codec.resolver.mux_video -> local.file.mux.codec",
                      blockingQueuePolicy(options.metadataQueueCapacity));
        graph.connect(split,
                      "video",
                      videoDecode,
                      "packet",
                      "local.stream.split.video -> local.video.decode.packet",
                      blockingQueuePolicy(options.packetQueueCapacity));
        graph.connect(videoDecode,
                      "frame",
                      videoFilter,
                      "frame",
                      "local.video.decode.frame -> local.video.filter.frame",
                      blockingQueuePolicy(options.frameQueueCapacity));
        graph.connect(videoFilter,
                      "frame",
                      videoTimestamp,
                      "frame",
                      "local.video.filter.frame -> local.video.timestamp.frame",
                      blockingQueuePolicy(options.frameQueueCapacity));
        graph.connect(videoTimestamp,
                      "frame",
                      videoEncode,
                      "frame",
                      "local.video.timestamp.frame -> local.video.encode.frame",
                      blockingQueuePolicy(options.frameQueueCapacity));
        graph.connect(videoEncode,
                      "packet",
                      mux,
                      "packet",
                      "local.video.encode.packet -> local.file.mux.packet",
                      blockingQueuePolicy(options.muxQueueCapacity));
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
