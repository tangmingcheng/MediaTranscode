#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpOutputNodePlan final {
    std::string url;
    int packetSize;
    std::string mediaId;
    bool writePacingEnabled = false;
    std::int64_t writePacingBytesPerSecond = 0;
    std::int64_t writePacingBurstBytes = 0;
    std::optional<MediaRtpUdpSenderConfig> scheduledTransport;
    std::optional<MediaScheduledRtpPacketizationPlan> scheduledPacketization;
};

struct MediaRealtimeMuxedOutputPlan final {
    std::string url;
    std::string format;
    std::string mediaId;
    std::optional<MediaOutputResourceKind> outputResourceKind;
    std::optional<MediaMuxSessionKind> muxSessionKind;
    std::optional<MediaRtpUdpSenderConfig> rtpTransport;
    std::string sdpPath;
};

struct MediaRealtimeSdpWriterPlan final {
    std::string path;
    std::string mediaId;
    int expectedContexts = 1;
};

struct MediaRealtimeMuxNodePlan final {
    bool expectVideo;
    bool expectAudio;
    MediaLatencyPolicy pacingPolicy;
    bool monotonicPacketTimestamps = false;
    int startupDelayMs = 0;
};

struct MediaRealtimeOutputPlanningDraft final {
    bool packetCopyNormalizationRequired = false;
    MediaRealtimeRtpOutputNodePlan videoOutput;
    MediaRealtimeRtpOutputNodePlan audioOutput;
    MediaRealtimeMuxedOutputPlan muxedOutput;
    MediaRealtimeSdpWriterPlan sdp;
    MediaRealtimeMuxNodePlan singleStreamMux;
};

} // namespace media::ffmpeg::graph
