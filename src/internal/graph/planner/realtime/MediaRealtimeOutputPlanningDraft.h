#pragma once

#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeScheduledRtpOutputPlanningDraft final {
    std::string url;
    int packetSize;
    std::string mediaId;
    bool writePacingEnabled = false;
    std::int64_t writePacingBytesPerSecond = 0;
    std::int64_t writePacingBurstBytes = 0;
    std::optional<MediaRtpUdpSenderConfig> scheduledTransport;
    std::optional<MediaScheduledRtpPacketizationPlan> scheduledPacketization;
};

struct MediaRealtimeMpegTsOutputPlanningDraft final {
    std::string url;
    std::string mediaId;
    std::optional<MediaRtpUdpSenderConfig> rtpTransport;
    std::string sdpPath;
    std::optional<std::int64_t> scheduledWireBytesPerSecond;
    std::optional<MediaRunningTime> transportDecodeLead;
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
