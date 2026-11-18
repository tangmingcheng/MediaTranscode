#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "media_transcode/Result.h"
#include "internal/graph/planner/realtime/MediaRtpInputWaitPlan.h"
#include "internal/graph/planner/realtime/MediaRtpVideoPreparationTiming.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <stop_token>

namespace media::ffmpeg::graph {

struct MediaRawRtpPreparedStreamPlan final {
    MediaRtpUdpTransportConfig transport;
    MediaPreparedRawRtpIdentity identity;
};

struct MediaRawRtpProbePlan final {
    explicit MediaRawRtpProbePlan(MediaRtpVideoPreparationIntent selectedIntent)
        : timingIntent(selectedIntent) {}
    MediaRtpVideoPreparationIntent timingIntent;
    struct VideoOnly final {
        MediaRawRtpPreparedStreamPlan video;
    };
    struct AudioVideo final {
        MediaRawRtpPreparedStreamPlan video;
        MediaRawRtpPreparedStreamPlan audio;
    };
    using Streams = std::variant<VideoOnly, AudioVideo>;

    Streams streams;
    int openTimeoutMs = 0;
    int analyzeDurationUs = 0;
    std::size_t maximumBufferedBytes = 0;
    std::optional<MediaRawRtpPreparationStoragePlan> storage;
    std::size_t reorderWindowPackets = 0;
    std::optional<MediaRtpInputWaitPlan> inputWait;
    std::optional<MediaRtpVideoSignalingFacts>
        authoritativeVideoSignaling;
    std::optional<MediaRtpVideoPacketizationPolicy> packetizationPolicy;
    std::optional<MediaRtcpCompoundPolicy> rtcpPolicy;
};

struct MediaPreparedRawRtpVideoOnlyProbe final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaVideoSourceTiming timing;
    MediaPreparedRealtimeInput video;
    std::shared_ptr<MediaRawRtpPreparedByteBudget> byteBudget;
    std::chrono::steady_clock::time_point preparationDeadline;
    MediaRtpInputWaitPlan inputWait;
};

struct MediaPreparedRawRtpAudioVideoProbe final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaVideoSourceTiming timing;
    MediaPreparedRealtimeInput video;
    MediaPreparedRealtimeInput audio;
    std::shared_ptr<MediaRawRtpPreparedByteBudget> byteBudget;
    std::chrono::steady_clock::time_point preparationDeadline;
    MediaRtpInputWaitPlan inputWait;
};

using MediaPreparedRawRtpProbe = std::variant<
    MediaPreparedRawRtpVideoOnlyProbe,
    MediaPreparedRawRtpAudioVideoProbe>;

class MediaRawRtpInputPreparer final {
public:
    static ::media::Result<MediaPreparedRawRtpProbe> prepare(
        const MediaRawRtpProbePlan& plan, std::stop_token cancellation,
        const MediaPreparationStorageScope& scope);
    static ::media::Status sealPreflight(MediaPreparedRawRtpProbe& probe);

private:
    MediaRawRtpInputPreparer() = delete;
};

} // namespace media::ffmpeg::graph
