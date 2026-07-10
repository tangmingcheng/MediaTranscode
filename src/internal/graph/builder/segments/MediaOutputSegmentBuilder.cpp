#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

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
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph,
                                                                   owner,
                                                                   segment.mux,
                                                                   MediaTranscodeOptionKey::MuxExpectVideo,
                                                                   boolOption(options.expectVideo)); !status) {
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
                                                                   "format",
                                                                   MediaStreamKind::Metadata,
                                                                   MediaEdgeKind::Metadata,
                                                                   MediaPayloadKind::FormatContext,
                                                                   true,
                                                                   false); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  segment.mux,
                                                                  "format",
                                                                  MediaStreamKind::Metadata,
                                                                  MediaEdgeKind::Metadata,
                                                                  MediaPayloadKind::FormatContext,
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
                                                                  MediaPayloadKind::Unknown,
                                                                  true,
                                                                  true); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  segment.mux,
                                                                  "packet",
                                                                  MediaStreamKind::Any,
                                                                  MediaEdgeKind::Unknown,
                                                                  MediaPayloadKind::Packet,
                                                                  true,
                                                                  true); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }

    if (auto status = MediaGraphBuildSupport::connectChecked(graph,
                                                            owner,
                                                            segment.fileOutput,
                                                            "format",
                                                            segment.mux,
                                                            "format",
                                                            options.prefix + ".output.format -> mux.format",
                                                            MediaGraphBuildSupport::blockingQueuePolicy(options.queues.metadata)); !status) {
        return ::media::Result<FileOutputSegment>::failure(status.error());
    }

    return ::media::Result<FileOutputSegment>::success(segment);
}

} // namespace media::ffmpeg::graph
