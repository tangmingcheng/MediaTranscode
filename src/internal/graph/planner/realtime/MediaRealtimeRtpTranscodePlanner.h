#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPlanningDraft.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"
#include "media_transcode/Result.h"

#include <string>
#include <optional>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputStreamInfo;
struct MediaDetectedRtpVideoSignaling;
struct MediaRealtimeRtpTranscodePlanCore {
    RealtimeInputType inputType;
    RealtimeInputStreamLayout inputLayout;
    RealtimeOutputStreamLayout outputLayout;
    MediaOutputTransportKind outputTransport;
    MediaPipelinePlan videoPlan;
    MediaVideoTranscodeParameters videoParameters;
    std::optional<MediaRealtimeGraphResourceLedgerPlan> resourceLedger;
    std::optional<MediaPreparedRealtimeInputKind> requiredPreparedInputKind;
    bool videoInputStartRequiresKeyFrame = false;
    MediaRealtimeRtpInputNodePlan input;
};

struct MediaRealtimeRtpTranscodePlanningDraft final
    : MediaRealtimeRtpTranscodePlanCore {
    std::optional<MediaRealtimeDeploymentEnvelope> deployment;
    std::optional<MediaAudioPipelinePlan> audioPlan;
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput;
    std::optional<MediaRealtimeAvSyncComponentBounds> avSyncComponentBounds;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
};

struct MediaRealtimeRtpTranscodePlan final : MediaRealtimeRtpTranscodePlanCore {
    MediaRealtimeRtpTranscodePlan() = delete;
    MediaRealtimeRtpTranscodePlan(
        MediaRealtimeRtpTranscodePlanningDraft draft,
        MediaRealtimeRuntimePlan selectedRuntime) noexcept
        : MediaRealtimeRtpTranscodePlanCore(std::move(draft)),
          runtime(std::move(selectedRuntime))
    {
    }

    MediaRealtimeRuntimePlan runtime;
};

struct MediaRealtimeTranscodePreflight final {
    MediaRealtimeTranscodePreflight() = delete;
    explicit MediaRealtimeTranscodePreflight(
        MediaRealtimeRtpTranscodePlan selectedPlan) noexcept
        : plan(std::move(selectedPlan))
    {
    }

    MediaRealtimeRtpTranscodePlan plan;
    std::optional<MediaPreparedRealtimeInput> prepared;
    std::optional<MediaPreparedRealtimeInput> preparedAudio;
};

class MediaRealtimeRtpTranscodePlanner final {
public:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflight(
        const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflight(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimePreflightIo& io);
    static ::media::Result<MediaRealtimeRtpTranscodePlan> planPreparedInput(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputStreamInfo& input,
        const MediaTsSelectedProgramPlan& selectedTsProgram);
    static ::media::Status validateRealtimeRequestNoIo(
        const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Status validatePlannedProduct(
        const MediaRealtimeRtpTranscodePlan& plan);

private:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> planWithInput(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputStreamInfo* inputInfo,
        const MediaTsSelectedProgramPlan* selectedTsProgram,
        const MediaPreparedRealtimeInput* preparedInput,
        const MediaPreparedRealtimeInput* preparedAudioInput,
        const MediaRtpIngressPlan* preparedVideoIngress,
        std::optional<MediaPipelinePlan> preplannedVideo,
        const MediaDetectedRtpVideoSignaling* detectedVideoSignaling,
        const MediaRational* detectedVideoFrameRate);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflightImpl(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimePreflightIo* io);
    MediaRealtimeRtpTranscodePlanner() = default;
};

} // namespace media::ffmpeg::graph
