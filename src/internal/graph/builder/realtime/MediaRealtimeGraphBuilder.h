#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaRealtimeGraphKind {
    PacketRelay,
    DecodeEncode,
    IngestToMux
};

struct MediaRealtimeGraphBuilderOptions {
    MediaRealtimeGraphKind kind = MediaRealtimeGraphKind::PacketRelay;

    std::string inputUrl;
    std::string outputUrl;
    std::string sdpPath;
    std::string mediaId;

    bool includeAudio = true;
    bool includeVideo = true;
    bool enablePacketFanout = true;
    bool enableRtpMux = false;
    bool enableSdpWriter = false;

    std::size_t queueCapacity = 8;
    std::size_t highWatermark = 6;
    std::size_t criticalWatermark = 8;
};

struct MediaRealtimeGraphBuilderResult {
    MediaGraph graph;
};

class MediaRealtimeGraphBuilder final {
public:
    static ::media::Result<MediaRealtimeGraphBuilderResult> build(
        const MediaRealtimeGraphBuilderOptions& options = {});

    static ::media::Result<MediaGraph> buildPacketRelay(
        const MediaRealtimeGraphBuilderOptions& options = {});

    static ::media::Result<MediaGraph> buildDecodeEncode(
        const MediaRealtimeGraphBuilderOptions& options = {});

    static ::media::Result<MediaGraph> buildIngestToMux(
        const MediaRealtimeGraphBuilderOptions& options = {});

private:
    static MediaEdgePolicy realtimeEdgePolicy(const MediaRealtimeGraphBuilderOptions& options) noexcept;
    static ::media::Result<void> applyRealtimeInputOptions(MediaGraph& graph,
                                                          MediaNodeId nodeId,
                                                          const MediaRealtimeGraphBuilderOptions& options);
    static ::media::Result<void> applyRealtimeOutputOptions(MediaGraph& graph,
                                                           MediaNodeId nodeId,
                                                           const MediaRealtimeGraphBuilderOptions& options);
};

} // namespace media::ffmpeg::graph
