#pragma once

#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpRemoteEndpointPair.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeScheduledRtpOutputPlanningDraft final {
    std::string url;
    int packetSize;
    std::string mediaId;
    std::optional<MediaRtpRemoteEndpointPair> scheduledTransport;
    std::optional<MediaScheduledRtpPacketizationPlan> scheduledPacketization;
};

struct MediaRealtimeMpegTsOutputPlanningDraft final {
    std::string url;
    std::string mediaId;
    std::optional<MediaRtpRemoteEndpointPair> rtpTransport;
    std::optional<std::size_t> maximumDatagramBytes;
    std::string sdpPath;
    std::optional<MediaRunningTime> transportDecodeLead;
    std::optional<MediaRunningTime> startupEmissionPreroll;
};

struct MediaRealtimeSeparateRtpSdpPlanningDraft final {
    std::string path;
    std::string mediaId;
};

struct MediaRealtimeOutputPlanningDraft final {
    bool packetCopyNormalizationRequired = false;
    MediaRealtimeScheduledRtpOutputPlanningDraft videoOutput;
    MediaRealtimeScheduledRtpOutputPlanningDraft audioOutput;
    MediaRealtimeMpegTsOutputPlanningDraft muxedOutput;
    MediaRealtimeSeparateRtpSdpPlanningDraft sdp;
};

} // namespace media::ffmpeg::graph
