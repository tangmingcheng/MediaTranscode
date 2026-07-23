#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAvSyncError.h"
#include "internal/graph/sync/MediaVideoSyncDecision.h"

#include <cstdint>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaVideoFrameMeasurement final {
    MediaRunningTime dispatchOnMaster;
    MediaRunningTime targetPresentationOnMaster;
    MediaRunningTime decisionHorizonOnMaster;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    bool keyFrame = false;
};

struct MediaVideoRepeatRequest final {
    MediaRunningTime repeatDispatchOnMaster;
    MediaRunningTime repeatPresentationOnMaster;
    MediaRunningTime lastEmittedPresentationOnMaster;
    MediaRunningTime decisionHorizonOnMaster;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
};

using MediaVideoSyncMeasurement =
    std::variant<MediaVideoFrameMeasurement, MediaVideoRepeatRequest>;

class MediaVideoSyncController final {
public:
    static MediaAvSyncResult<MediaVideoSyncController> create(
        const MediaAvSyncPlan& plan,
        std::uint64_t generation);

    MediaAvSyncResult<MediaVideoSyncDecision> update(
        const MediaVideoSyncMeasurement& measurement);
    MediaAvSyncStatus reset(std::uint64_t generation);

private:
    struct Policy final {
        std::int64_t earlyHoldThresholdNs = 0;
        std::int64_t lateDisplayThresholdNs = 0;
        std::int64_t dropThresholdNs = 0;
        bool allowRecoveryRepeat = false;
        int maximumConsecutiveRecoveryActions = 0;
        std::int64_t hardDiscontinuityThresholdNs = 0;
    };

    MediaVideoSyncController(MediaAvSyncTopology topology,
                             Policy policy,
                             std::uint64_t generation) noexcept;

    MediaAvSyncResult<MediaVideoSyncDecision> updateFrame(
        const MediaVideoFrameMeasurement& measurement);
    MediaAvSyncResult<MediaVideoSyncDecision> updateRepeat(
        const MediaVideoRepeatRequest& request);
    MediaAvSyncResult<MediaVideoSyncDecision> recoveryDecision(
        MediaVideoSyncDecisionKind kind,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime phaseError,
        std::uint64_t sequence);
    MediaAvSyncResult<MediaVideoSyncDecision> isolatedGenerationDecision(
        MediaRunningTime presentationOnMaster,
        MediaRunningTime phaseError,
        std::uint64_t observedGeneration,
        std::uint64_t sequence) const;
    MediaAvSyncStatus validateSequence(std::uint64_t sequence) const;
    MediaAvSyncStatus validateFrameIdentity(
        const MediaVideoFrameMeasurement& measurement) const;
    MediaAvSyncError error(MediaAvSyncErrorCode code,
                           const char* operation,
                           std::uint64_t observedGeneration,
                           MediaRunningTime presentationOnMaster,
                           const char* detail) const;
    MediaVideoSyncDecision decision(MediaVideoSyncDecisionKind kind,
                                    MediaRunningTime presentationOnMaster,
                                    MediaRunningTime phaseError,
                                    std::uint64_t sequence,
                                    std::optional<MediaVideoReacquisitionCause>
                                        reacquisitionCause =
                                            std::nullopt) const noexcept;

    MediaAvSyncTopology m_topology;
    Policy m_policy;
    std::uint64_t m_generation = 0;
    std::uint64_t m_lastSequence = 0;
    struct HeldFrameIdentity final {
        std::uint64_t sequence;
        MediaRunningTime dispatchOnMaster;
        MediaRunningTime targetPresentationOnMaster;
        bool keyFrame;
    };
    std::optional<HeldFrameIdentity> m_heldFrame;
    int m_consecutiveRecoveryActions = 0;
};

} // namespace media::ffmpeg::graph
