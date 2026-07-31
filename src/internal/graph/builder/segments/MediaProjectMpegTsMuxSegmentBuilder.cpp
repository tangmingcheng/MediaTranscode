#include "internal/graph/builder/segments/MediaProjectMpegTsMuxSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner =
    "MediaProjectMpegTsMuxSegmentBuilder";

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

} // namespace

::media::Result<MediaNodeId>
MediaProjectMpegTsMuxSegmentBuilder::build(
    MediaGraph& graph,
    const MediaProjectMpegTsMuxSegmentOptions& options)
{
    if (options.prefix.empty() ||
        (!options.expectVideo && !options.expectAudio) ||
        options.sessionKind != MediaMuxSessionKind::ProjectMpegTs) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux segment requires complete planner assembly facts"));
    }
    auto sessionKind =
        mediaMuxSessionKindOptionValue(options.sessionKind);
    if (!sessionKind) {
        return ::media::Result<MediaNodeId>::failure(
            sessionKind.error());
    }
    const MediaNodeId mux = graph.addNode(
        MediaNodeKind::FileMux,
        options.prefix + ".mux",
        "Project MPEG-TS mux");
    if (!mux.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError(
                "Project MPEG-TS mux segment failed to add its mux"));
    }
    for (const auto& [key, value] : {
             std::pair{MediaTranscodeOptionKey::MuxExpectVideo,
                       std::string(boolOption(options.expectVideo))},
             std::pair{MediaTranscodeOptionKey::MuxExpectAudio,
                       std::string(boolOption(options.expectAudio))},
             std::pair{MediaTranscodeOptionKey::MuxSessionKind,
                       std::move(sessionKind).value()}}) {
        auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, mux, key, value);
        if (!set) {
            return ::media::Result<MediaNodeId>::failure(set.error());
        }
    }
    if (options.requireByteSinkResource) {
        auto added = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, mux, "resource",
            MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
            MediaPayloadKind::OutputByteSink, true, false);
        if (!added) {
            return ::media::Result<MediaNodeId>::failure(added.error());
        }
    }
    if (auto added = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, mux, "codec", MediaStreamKind::Any,
            MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext,
            true, true); !added) {
        return ::media::Result<MediaNodeId>::failure(added.error());
    }
    if (auto added = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, mux, "packet", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket,
            MediaPayloadKind::TsAccessUnit, true, true); !added) {
        return ::media::Result<MediaNodeId>::failure(added.error());
    }
    if (auto added = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, mux, "plan", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan,
            true, false); !added) {
        return ::media::Result<MediaNodeId>::failure(added.error());
    }
    return ::media::Result<MediaNodeId>::success(mux);
}

} // namespace media::ffmpeg::graph
