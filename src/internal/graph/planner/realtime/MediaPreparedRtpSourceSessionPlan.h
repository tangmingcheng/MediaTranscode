#pragma once
#include "internal/graph/planner/realtime/MediaPreparedRtpStorageAdmission.h"
#include "internal/graph/planner/realtime/MediaRtcpPreparationStoragePlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceFactsPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSourceLifecyclePlan.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedInputRetentionPlan.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpAdmissionPlan.h"
#include "internal/graph/planner/realtime/MediaSourcePreparationStoragePlan.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupSession.h"
#include "internal/graph/protocol/rtp/MediaRtpInputClockSession.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/runtime/session/MediaRtpPacketClockSession.h"

namespace media::ffmpeg::graph {

struct MediaPreparedRtpSourceSessionStreamPlan final {
    MediaRtpDepacketizerConfig depacketizer;
    MediaPreparedRtpAccessUnitEnvelope envelope;
    MediaRtpReorderConfig reorder;
    MediaRtpInputClockSessionConfig clock;
    MediaRtpProtocolPreparationStoragePlan workingSet;
    MediaRtpPacketClockSessionConfig binder;
    std::size_t maximumProtocolOutputObjects;
    std::size_t maximumProtocolOutputBytes;
    MediaPreparedRtpContainerStoragePlan containerStorage;
    // Residual legacy protocol owners; actual migrated requests are excluded.
    MediaRtpProtocolStorageCosts unmigratedProtocolStorage{};
};

struct MediaPreparedRtpSourceSessionPlan final {
    MediaPreparedRtpSourceSessionStreamPlan video;
    std::optional<MediaPreparedRtpSourceSessionStreamPlan> audio;
    MediaRtpClockGroupSessionConfig clockGroup;
    MediaPreparedRtpAdmissionPlan admission;
    std::optional<MediaPreparedInputRetentionPlan> canonicalRuntimeRetention;
    std::optional<MediaPreparedInputRetentionStream> audioRetention;
    MediaAvSourceLifecycleMode lifecycleMode;
    std::uint64_t initialGeneration;
    std::chrono::steady_clock::time_point deadline;
    std::size_t fixedStorageBytes;
    std::size_t storageBytes;
    // Published only by the lock-held freeze transaction.
    std::optional<MediaPreparedRtpStorageAdmission> sealedAdmission;
    std::optional<MediaRtcpPreparationStoragePlan> rtcpStorage;
    // Actual resource allocations, not part of the legacy fixed lease.
    std::optional<std::size_t> fixedOwnerAllocationBytes;
    std::optional<MediaRtpVideoSignalingFacts> videoSignaling;

    ::media::Status validateAudioRetention() const;
};

class MediaPreparedRtpSourceSessionPlanner final {
public:
    static ::media::Result<MediaPreparedRtpSourceSessionPlan> plan(
        const MediaRealtimeCompositionSourceFacts& facts,
        const MediaPreparedRtpIngressCandidate& video,
        const MediaPreparedRtpIngressCandidate* audio,
        std::uint64_t videoPrefixAccessUnitBound,
        std::optional<std::uint64_t> audioPrefixAccessUnitBound,
        std::chrono::steady_clock::time_point deadline,
        std::chrono::steady_clock::time_point observedNow,
        std::size_t sharedCapacityBytes);
};

} // namespace media::ffmpeg::graph
