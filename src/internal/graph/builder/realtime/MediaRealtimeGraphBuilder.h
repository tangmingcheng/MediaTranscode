#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRealtimeGraphKind {
    PacketRelay,
    IngestToMux,
    RtpTranscode
};

struct MediaRealtimeInputConfig {
    std::string url;
    std::string rtspTransport = "tcp";
    int openTimeoutMs = 5000;
    int readTimeoutMs = 5000;
    int analyzeDurationUs = 500000;
    int probeSizeBytes = 512 * 1024;
    bool lowLatency = true;
    int videoStreamIndex = invalidMediaStreamIndex;
};

struct MediaRealtimeOutputConfig {
    std::string host = "127.0.0.1";
    std::size_t basePort = 5004;
    std::string sdpPath;
    std::string url;
    int packetSize = 1200;
};

struct MediaRealtimeGraphBuilderOptions {
    MediaRealtimeGraphKind kind = MediaRealtimeGraphKind::PacketRelay;

    MediaRealtimeInputConfig input;
    MediaRealtimeOutputConfig output;
    MediaTranscodeParameterSet parameters;

    std::string mediaId;

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

private:
    MediaRealtimeGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
