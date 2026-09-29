#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/planner/realtime/MediaRawRtpInputPreparer.h"
#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"

#include <chrono>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputConfig;

struct MediaRtpSourceObservation final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaRational frameRate;
};

struct MediaPreparedRtpSourceInputs final {
    MediaPreparedRealtimeInput video;
    std::optional<MediaPreparedRealtimeInput> audio;
};

// Borrowed only until the sealed owner is moved, released or destroyed.
struct MediaPreparedRtpSourceView final {
    const MediaPreparedRealtimeInput& video;
    const MediaPreparedRealtimeInput* audio;
    const MediaRtpIngressPlan& videoIngress;
    const MediaRtpIngressPlan* audioIngress;
};

class MediaPreparedRtpSource final {
public:
    MediaPreparedRtpSource(const MediaPreparedRtpSource&) = delete;
    MediaPreparedRtpSource& operator=(const MediaPreparedRtpSource&) = delete;
    MediaPreparedRtpSource(MediaPreparedRtpSource&& other) noexcept;
    MediaPreparedRtpSource& operator=(MediaPreparedRtpSource&&) = delete;

    ::media::Result<MediaPreparedRtpSourceView> resources() const;
    ::media::Result<MediaPreparedRtpSourceInputs> release() &&;

private:
    friend class MediaRtpSourcePreflight;
    MediaPreparedRtpSource(MediaPreparedRawRtpProbe probe,
        MediaRtpIngressPlan videoIngress, std::optional<MediaRtpIngressPlan> audioIngress,
        std::chrono::steady_clock::time_point deadline);

    std::optional<MediaPreparedRawRtpProbe> m_probe;
    MediaRtpIngressPlan m_videoIngress;
    std::optional<MediaRtpIngressPlan> m_audioIngress;
    std::chrono::steady_clock::time_point m_deadline;
};

// Input-only preparation: no encoder, output URL, graph ledger or output runtime.
class MediaRtpSourcePreflight final {
public:
    MediaRtpSourcePreflight(const MediaRtpSourcePreflight&) = delete;
    MediaRtpSourcePreflight& operator=(const MediaRtpSourcePreflight&) = delete;
    MediaRtpSourcePreflight(MediaRtpSourcePreflight&& other) noexcept;
    MediaRtpSourcePreflight& operator=(MediaRtpSourcePreflight&&) = delete;

    static ::media::Result<MediaRtpSourcePreflight> begin(
        const MediaRealtimeInputConfig& input, MediaTranscodeStreamSet streamSet,
        std::chrono::steady_clock::time_point deadline);
    ::media::Result<MediaRtpSourceObservation> observation() const;
    ::media::Result<MediaPreparedRtpSource> seal() &&;

private:
    MediaRtpSourcePreflight(MediaPreparedRawRtpProbe probe,
        std::chrono::steady_clock::time_point deadline);
    std::optional<MediaPreparedRawRtpProbe> m_probe;
    std::chrono::steady_clock::time_point m_deadline;
};

} // namespace media::ffmpeg::graph
