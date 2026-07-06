#include "internal/graph/builder/realtime/MediaRealtimeIngestToMuxGraphBuilder.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* unsupportedReason =
    "MediaRealtimeIngestToMuxGraphBuilder: realtime ingest-to-mux uses legacy packet ports and is disabled for the current realtime runtime";

} // namespace

::media::Result<MediaGraph> MediaRealtimeIngestToMuxGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    (void)options;
    return ::media::Result<MediaGraph>::failure(
        ::media::ErrorInfo::unsupported(unsupportedReason));
}

} // namespace media::ffmpeg::graph
