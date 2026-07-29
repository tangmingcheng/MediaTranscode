#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"

namespace media::ffmpeg::graph {
namespace {

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

} // namespace

::media::Result<FileOutputSegment> MediaOutputSegmentBuilder::buildFileMuxOutput(
    MediaGraph& graph,
    const FileOutputSegmentOptions& options)
{
    constexpr const char* owner = "MediaOutputSegmentBuilder";
    if (options.outputUrl.empty()) {
        return ::media::Result<FileOutputSegment>::failure(
            ::media::ErrorInfo::invalidArgument("MediaOutputSegmentBuilder requires outputUrl"));
    }
    if (options.queues.metadata == 0) {
        return ::media::Result<FileOutputSegment>::failure(
            ::media::ErrorInfo::invalidArgument("MediaOutputSegmentBuilder metadata queue capacity must be greater than 0"));
    }
    if (!options.muxSessionKind) {
        return ::media::Result<FileOutputSegment>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaOutputSegmentBuilder requires muxSessionKind"));
    }
    if (!options.outputResourceKind) {
        return ::media::Result<FileOutputSegment>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaOutputSegmentBuilder requires outputResourceKind"));
    }
    if (!options.expectVideo && !options.expectAudio) {
        return ::media::Result<FileOutputSegment>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaOutputSegmentBuilder requires at least one mux stream"));
    }
    auto sessionKind = mediaMuxSessionKindOptionValue(*options.muxSessionKind);
    if (!sessionKind) {
        return ::media::Result<FileOutputSegment>::failure(sessionKind.error());
    }
    auto resourceKind = mediaOutputResourceKindOptionValue(
        *options.outputResourceKind);
    if (!resourceKind) {
        return ::media::Result<FileOutputSegment>::failure(resourceKind.error());
    }
    const bool ffmpegPair =
        *options.outputResourceKind == MediaOutputResourceKind::FFmpegFormatContext &&
        *options.muxSessionKind == MediaMuxSessionKind::FFmpegFile;
    const bool projectPair =
        *options.outputResourceKind == MediaOutputResourceKind::ByteSink &&
        *options.muxSessionKind == MediaMuxSessionKind::ProjectMpegTs;
    if (!ffmpegPair && !projectPair) {
        return ::media::Result<FileOutputSegment>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaOutputSegmentBuilder output resource and mux session kinds conflict"));
    }
    const MediaPayloadKind resourcePayload = ffmpegPair
        ? MediaPayloadKind::FormatContext
        : MediaPayloadKind::OutputByteSink;
    const MediaPayloadKind codecPayload = ffmpegPair
        ? MediaPayloadKind::Unknown
        : MediaPayloadKind::CodecContext;
    const MediaPayloadKind packetPayload = ffmpegPair
        ? MediaPayloadKind::Packet
        : MediaPayloadKind::TsAccessUnit;
    FileOutputSegment segment;
    segment.fileOutput = graph.addNode(MediaNodeKind::FileOutput,
                                       options.prefix + ".output",
                                       "File output segment");
    segment.mux = graph.addNode(MediaNodeKind::FileMux,
                                options.prefix + ".mux",
                                "File mux segment");

    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, segment.fileOutput, "url", options.outputUrl); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (!options.outputFormat.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, segment.fileOutput, "format", options.outputFormat); !status) {
            return ::media::Result<FileOutputSegment>::failure(status.error());
        }
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph,
            owner,
            segment.fileOutput,
            MediaTranscodeOptionKey::OutputResourceKind,
            resourceKind.value()); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph,
                                                                   owner,
                                                                   segment.mux,
                                                                   MediaTranscodeOptionKey::MuxExpectVideo,
                                                                   boolOption(options.expectVideo)); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph,
            owner,
            segment.mux,
            MediaTranscodeOptionKey::MuxSessionKind,
            sessionKind.value()); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph,
                                                                   owner,
                                                                   segment.mux,
                                                                   MediaTranscodeOptionKey::MuxExpectAudio,
                                                                   boolOption(options.expectAudio)); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }

    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                                   owner,
                                                                   segment.fileOutput,
                                                                   "resource",
                                                                   MediaStreamKind::Metadata,
                                                                   MediaEdgeKind::Metadata,
                                                                   resourcePayload,
                                                                   true,
                                                                   false); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  segment.mux,
                                                                  "resource",
                                                                  MediaStreamKind::Metadata,
                                                                  MediaEdgeKind::Metadata,
                                                                  resourcePayload,
                                                                  true,
                                                                  false); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  segment.mux,
                                                                  "codec",
                                                                  MediaStreamKind::Any,
                                                                  MediaEdgeKind::Metadata,
                                                                  codecPayload,
                                                                  true,
                                                                  true); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  segment.mux,
                                                                  "packet",
                                                                  MediaStreamKind::Any,
                                                                  MediaEdgeKind::EncodedPacket,
                                                                  packetPayload,
                                                                  true,
                                                                  true); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }

    if (auto status = MediaGraphBuildSupport::connectChecked(graph,
                                                            owner,
                                                            segment.fileOutput,
                                                            "resource",
                                                            segment.mux,
                                                            "resource",
                                                            options.prefix + ".output.resource -> mux.resource",
                                                            MediaBlockingEdgePolicyPlanner::planQueue(options.queues.metadata)); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }

    if (projectPair) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                graph,
                owner,
                segment.mux,
                "plan",
                MediaStreamKind::Metadata,
                MediaEdgeKind::Metadata,
                MediaPayloadKind::TsMuxRuntimePlan,
                true,
                false); !status) {
            return ::media::Result<FileOutputSegment>::failure(status.error());
        }
    }

    return ::media::Result<FileOutputSegment>::success(segment);
}

} // namespace media::ffmpeg::graph
