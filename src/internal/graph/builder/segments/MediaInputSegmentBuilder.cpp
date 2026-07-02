#include "internal/graph/builder/segments/MediaInputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

namespace media::ffmpeg::graph {

::media::Result<FileInputSegment> MediaInputSegmentBuilder::buildFileInput(
    MediaGraph& graph,
    const FileInputSegmentOptions& options)
{
    constexpr const char* owner = "MediaInputSegmentBuilder";
    if (options.inputUrl.empty()) {
        return ::media::Result<FileInputSegment>::failure(
            ::media::ErrorInfo::invalidArgument("MediaInputSegmentBuilder requires inputUrl"));
    }

    FileInputSegment segment;
    segment.input = graph.addNode(MediaNodeKind::FileInput,
                                  options.prefix + ".input",
                                  "File input segment");

    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, segment.input, "url", options.inputUrl); !status) {
        return ::media::Result<FileInputSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                                   owner,
                                                                   segment.input,
                                                                   segment.formatPort,
                                                                   MediaStreamKind::Metadata,
                                                                   MediaEdgeKind::Metadata,
                                                                   MediaPayloadKind::FormatContext,
                                                                   true,
                                                                   true); !status) {
        return ::media::Result<FileInputSegment>::failure(status.error());
    }

    return ::media::Result<FileInputSegment>::success(segment);
}

} // namespace media::ffmpeg::graph
