#pragma once
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"
#include "internal/graph/planner/realtime/MediaSourceLineageStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedPacketWrapperStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedFrameWrapperStoragePlanner.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"

namespace media::ffmpeg::graph {
struct MediaPreparedStreamCandidateCostFacts final {
    MediaPreparedRtpContainerBounds containers;
};
struct MediaPreparedSourceCandidateCostFacts final {
    MediaPreparedStreamCandidateCostFacts video;
    std::optional<MediaPreparedStreamCandidateCostFacts> audio;
    std::size_t startupVideoUnits;
    std::optional<std::size_t> startupAudioUnits;
    const MediaVideoSourcePlan& videoPlan;
    const MediaCompositionGraphTopology& topology;
    const MediaCompositionReferenceResidencePlan& residence;
    std::size_t sourceIndex;
    std::uint64_t registryCapacity;
};
enum class MediaPreparedPayloadLeaseRootKind {
    Prefix, Acquiring, Outputs, ProtocolPending, RawInputCreditWait, BinderCreditWait, Count
};
struct MediaPreparedPayloadLeaseRoot final {
    MediaPreparedPayloadLeaseRootKind kind;
    std::size_t maximumControls;
};
struct MediaPreparedStreamCandidateCosts final {
    // Original control field remains available for freeze validation, but is
    // excluded from containerBytes and replaced by all-live leaseControlsBytes.
    MediaPreparedRtpContainerStoragePlan containers;
    std::size_t containerBytes;
    std::size_t leaseControlsBytes;
    MediaPreparedPacketWrapperStoragePlan packetWrappers;
    std::array<MediaPreparedPayloadLeaseRoot,
        static_cast<std::size_t>(MediaPreparedPayloadLeaseRootKind::Count)> leaseControlRoots;
};
struct MediaPreparedSourceCandidateCosts final {
    std::size_t fixedBytes;
    MediaPreparedStreamCandidateCosts video;
    std::optional<MediaPreparedStreamCandidateCosts> audio;
    std::size_t startupIndexBytes;
    std::size_t startupGenerationBytes;
    MediaPreparedFrameWrapperStoragePlan frameWrappers;
    MediaSourceLineageStoragePlan lineage;
    // Deliberately no total/admission: payloads, route wrappers, reservations,
    // frame ownership overlap and clock evidence remain separate products.
};
class MediaPreparedSourceCandidateCostPlanner final {
public:
    static ::media::Result<MediaPreparedSourceCandidateCosts> plan(
        const MediaPreparedRtpSourceSessionPlan& session,
        const MediaPreparedRtpStorageSnapshot& held,
        const MediaPreparedSourceCandidateCostFacts& facts);
};
}
