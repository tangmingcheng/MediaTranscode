#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

#include "internal/graph/builder/realtime/MediaRealtimeIngestToMuxGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimePacketRelayGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeGraphBuilderResult> MediaRealtimeGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    ::media::Result<MediaGraph> graphResult = ::media::Result<MediaGraph>::failure(
        ::media::ErrorInfo::unsupported("unsupported realtime graph kind"));

    switch (options.kind) {
    case MediaRealtimeGraphKind::PacketRelay:
        graphResult = MediaRealtimePacketRelayGraphBuilder::build(options);
        break;
    case MediaRealtimeGraphKind::IngestToMux:
        graphResult = MediaRealtimeIngestToMuxGraphBuilder::build(options);
        break;
    case MediaRealtimeGraphKind::RtpTranscode:
        graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
        break;
    }

    if (!graphResult) {
        return ::media::Result<MediaRealtimeGraphBuilderResult>::failure(graphResult.error());
    }

    MediaRealtimeGraphBuilderResult result;
    result.graph = std::move(graphResult).value();
    return ::media::Result<MediaRealtimeGraphBuilderResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
