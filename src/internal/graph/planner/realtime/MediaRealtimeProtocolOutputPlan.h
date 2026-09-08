#pragma once

#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/planner/realtime/MediaMpegTsRtpOutputPlan.h"
#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"
#include "internal/graph/planner/realtime/MediaScheduledRtpOutputPlan.h"
#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

enum class MediaRtpSdpSessionIdPolicy {
    SharedNtpEpoch
};

enum class MediaRtpSdpSessionVersionPolicy {
    ActivePlaybackGeneration
};

struct MediaSeparateRtpSdpRuntimePlan final {
    std::string path;
    std::string originUsername;
    std::string sessionName;
    MediaIpAddressFamily originAddressFamily;
    std::string originNumericAddress;
    std::string cname;
    MediaRtpSdpSessionIdPolicy sessionIdPolicy;
    MediaRtpSdpSessionVersionPolicy sessionVersionPolicy;
    friend bool operator==(const MediaSeparateRtpSdpRuntimePlan&,
                           const MediaSeparateRtpSdpRuntimePlan&) = default;
};

struct MediaVideoOnlySeparateRtpOutputRuntimePlan final {
    MediaScheduledRtpOutputPlan video;
    MediaSeparateRtpSdpRuntimePlan sdp;
};

struct MediaSeparateRtpOutputRuntimePlan final {
    MediaScheduledRtpOutputPlan video;
    MediaScheduledRtpOutputPlan audio;
    MediaSeparateRtpSdpRuntimePlan sdp;
};

struct MediaMpegTsUdpOutputPlan final {
    std::string url;
    MediaOutputResourceKind resourceKind;
    MediaMuxSessionKind muxSessionKind;
};

struct MediaProjectMpegTsRuntimeOutputPlan final {
    MediaProjectMpegTsOutputPlan protocol;
    MediaMuxSessionKind muxSessionKind;
    MediaTsDatagramEmissionPlan emission;
    std::uint64_t scheduledBatchMaximumBytes;
    std::variant<MediaMpegTsUdpOutputPlan,
                 MediaMpegTsRtpOutputPlan> transport;
};

::media::Result<MediaProjectMpegTsRuntimeOutputPlan>
cloneMediaProjectMpegTsRuntimeOutputPlan(
    const MediaProjectMpegTsRuntimeOutputPlan& source);

} // namespace media::ffmpeg::graph
