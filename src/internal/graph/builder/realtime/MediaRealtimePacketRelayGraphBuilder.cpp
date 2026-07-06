#include "internal/graph/builder/realtime/MediaRealtimePacketRelayGraphBuilder.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* unsupportedReason =
    "MediaRealtimePacketRelayGraphBuilder: realtime packet relay uses legacy packet ports and is disabled for the current realtime runtime";

} // namespace

::media::Result<MediaGraph> MediaRealtimePacketRelayGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    (void)options;
    return ::media::Result<MediaGraph>::failure(
        ::media::ErrorInfo::unsupported(unsupportedReason));
}

} // namespace media::ffmpeg::graph
