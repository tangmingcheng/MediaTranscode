#pragma once
#include "internal/graph/planner/realtime/MediaCompositionPayloadResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpContainerStoragePlan.h"
namespace media::ffmpeg::graph {
enum class MediaPreparedPacketWrapperRootKind {
    GraphHolder, Prefix, Acquiring, Outputs, AtomicCompletion, CreditWait, Construction
};
struct MediaPreparedPacketWrapperRoot final {
    std::size_t creatorPosition;
    MediaPreparedPacketWrapperRootKind kind;
    std::optional<MediaPayloadResidenceOwner> holder;
    std::uint64_t maximumWrappers;
};
struct MediaPreparedPacketWrapperStoragePlan final {
    std::size_t sourceIndex;
    MediaStreamKind stream;
    std::vector<MediaPreparedPacketWrapperRoot> roots;
    std::uint64_t maximumWrappers;
    std::uint64_t preparationBytes;
};
// Raw FFmpegPacketBuffer allocations only, not AVPacket/payload/lease controls
// or the outer MediaVideoDecodeBuffer. Video uses source presentation timing.
class MediaPreparedPacketWrapperStoragePlanner final {
public:
    // Reuses existing evidence. Successful evaluation allocates no storage;
    // failure leaves the previous numeric costs unchanged.
    static ::media::Status evaluate(MediaPreparedPacketWrapperStoragePlan& plan);
    static ::media::Result<MediaPreparedPacketWrapperStoragePlan> plan(
        const MediaCompositionGraphTopology& topology,
        const MediaCompositionReferenceResidencePlan& reference,
        std::size_t sourceIndex, MediaStreamKind stream,
        const MediaPreparedRtpContainerBounds& containers, std::uint64_t maximumAtomicCompletion);
};
}
