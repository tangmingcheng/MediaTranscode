#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeGraphBuilderResult> MediaRealtimeGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    switch (options.kind) {
    case MediaRealtimeGraphKind::PacketRelay:
        return ::media::Result<MediaRealtimeGraphBuilderResult>::failure(
            ::media::ErrorInfo::unsupported("MediaRealtimeGraphBuilder PacketRelay is superseded by realtime-rtp"));
    case MediaRealtimeGraphKind::IngestToMux:
        return ::media::Result<MediaRealtimeGraphBuilderResult>::failure(
            ::media::ErrorInfo::unsupported("MediaRealtimeGraphBuilder IngestToMux is superseded by realtime-rtp"));
    case MediaRealtimeGraphKind::RtpTranscode:
        {
            auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
            if (!graphResult) {
                return ::media::Result<MediaRealtimeGraphBuilderResult>::failure(graphResult.error());
            }

            MediaRealtimeGraphBuilderResult result;
            result.graph = std::move(graphResult).value();
            return ::media::Result<MediaRealtimeGraphBuilderResult>::success(std::move(result));
        }
    }

    return ::media::Result<MediaRealtimeGraphBuilderResult>::failure(
        ::media::ErrorInfo::unsupported("unsupported realtime graph kind"));
}

} // namespace media::ffmpeg::graph
