#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaStreamKind.h"
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

enum class MediaRealtimeInputMode {
    Url,
    RawRtp
};

struct MediaRtpCodecHint {
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    std::string codecName;
    int payloadType = -1;
    int clockRate = 0;
    int channels = 0;
    std::string fmtp;
};

struct MediaRealtimeInputConfig {
    MediaRealtimeInputMode mode = MediaRealtimeInputMode::Url;
    std::string url;
    std::string sdpText;
    std::string sdpPath;
    std::vector<MediaRtpCodecHint> codecHints;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int readTimeoutMs = 5000;
    bool reconnect = true;
    int maxReconnectAttempts = 3;
};

struct MediaRtpOutputConfig {
    std::string host = "127.0.0.1";
    std::size_t basePort = 5004;
    std::size_t videoRtpPort = 0;
    std::size_t videoRtcpPort = 0;
    std::size_t audioRtpPort = 0;
    std::size_t audioRtcpPort = 0;
    std::string sdpPath;
    int ttl = 1;
    int writeTimeoutMs = 5000;
};

struct MediaRealtimeTranscodeOptions {
    MediaRealtimeInputConfig input;
    MediaRtpOutputConfig output;
    MediaTranscodeParameterSet parameters;
    MediaLatencyPolicy latency;
    bool diagnosticsEnabled = true;
};

struct MediaRealtimeGraphBuilderOptions {
    MediaRealtimeGraphKind kind = MediaRealtimeGraphKind::PacketRelay;

    MediaRealtimeInputConfig input;
    MediaRtpOutputConfig output;
    MediaTranscodeParameterSet parameters;
    MediaLatencyPolicy latency;

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

private:
    MediaRealtimeGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
