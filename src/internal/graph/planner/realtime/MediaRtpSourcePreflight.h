#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/MediaPreparedRtpStorageReservation.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/planner/realtime/MediaRawRtpInputPreparer.h"
#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"

#include <chrono>
#include <span>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputConfig;
struct MediaRealtimeCompositionSourceFacts;
struct MediaPreparedInputRetentionPlan;
struct MediaPreparedInputRetentionStream;
class MediaPreparedRtpSourceSession;
class MediaMasterClock;
class MediaNodeWakeup;
struct MediaPreparedRtpStorageAdmission;

struct MediaRtpSourceObservation final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaVideoSourceTiming timing;
};

struct MediaCapturingRtpSourceView final {
    const MediaPreparedRealtimeInput& video;
    const MediaPreparedRealtimeInput* audio;
};

// Borrowed from an installed continuous preflight until it is moved or sealed.
struct MediaContinuousRtpIngressView final {
    const MediaRtpIngressPlan& video;
    const MediaRtpIngressPlan* audio;
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
    const MediaPreparedInputRetentionPlan* continuousRetention;
    const MediaPreparedInputRetentionStream* continuousAudioRetention;
    std::optional<MediaPreparedRtpStorageReservation> continuousStorage;
    std::shared_ptr<MediaPreparationMemoryResource> continuousVideoResource;
    const MediaPreparedRtpStorageAdmission* continuousAdmission;
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
        std::chrono::steady_clock::time_point deadline,
        std::shared_ptr<MediaPreparedRtpSourceSession> continuousSource);

    std::optional<MediaPreparedRawRtpProbe> m_probe;
    MediaRtpIngressPlan m_videoIngress;
    std::optional<MediaRtpIngressPlan> m_audioIngress;
    std::chrono::steady_clock::time_point m_deadline;
    std::shared_ptr<MediaPreparedRtpSourceSession> m_continuousSource;
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
        std::chrono::steady_clock::time_point deadline, std::stop_token cancellation,
        const MediaPreparationStorageScope& scope, MediaRtpVideoPreparationIntent timingIntent);
    ::media::Result<MediaRtpSourceObservation> observation() const;
    ::media::Result<MediaCapturingRtpSourceView> resources() const;
    ::media::Status waitForVideoData(std::uint64_t previousIngressFrontier,
        std::stop_token cancellation);
    ::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>> installContinuousSession(
        const MediaRealtimeCompositionSourceFacts& facts,
        const std::shared_ptr<MediaMasterClock>& masterClock, std::stop_token cancellation,
        std::span<const std::shared_ptr<MediaNodeWakeup>> creditWakeups);
    ::media::Result<MediaContinuousRtpIngressView> continuousIngress() const;
    ::media::Status validateContinuousPreparation(
        const std::shared_ptr<MediaPreparedRtpSourceSession>& source) const;
    ::media::Result<MediaPreparedRtpSource> seal() &&;

private:
    MediaRtpSourcePreflight(MediaPreparedRawRtpProbe probe,
        std::chrono::steady_clock::time_point deadline,
        std::optional<MediaPreparedRtpIngressCandidate> videoCandidate,
        std::optional<MediaPreparedRtpIngressCandidate> audioCandidate,
        MediaPreparationStorageScope scope);
    MediaPreparationStorageScope m_storageScope;
    std::optional<MediaPreparedRawRtpProbe> m_probe;
    std::chrono::steady_clock::time_point m_deadline;
    std::shared_ptr<MediaPreparedRtpSourceSession> m_continuousSource;
    std::optional<MediaPreparedRtpIngressCandidate> m_videoCandidate;
    std::optional<MediaPreparedRtpIngressCandidate> m_audioCandidate;
};

} // namespace media::ffmpeg::graph
