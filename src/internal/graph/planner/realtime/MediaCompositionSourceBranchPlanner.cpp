#include "internal/graph/planner/realtime/MediaCompositionSourceBranchPlanner.h"

namespace media::ffmpeg::graph {
namespace {
template<bool Updating> ::media::Status compileBranches(
    const MediaVideoSourceTiming& timing, bool filterActive, bool hasAudio,
    const MediaRealtimeEdgePolicySet& policies, MediaCompositionSourceBranchPlan& result)
{
    const bool canonical = std::holds_alternative<MediaObservedVideoCadence>(timing);
    const auto decoderShape = MediaVideoBranchConnectionsPlanner::sourceDecoder();
    const auto frameShape = MediaVideoBranchConnectionsPlanner::sourceFrames(canonical, filterActive);
    const auto audioShape = MediaAudioBranchConnectionsPlanner::source(false, true, true);
    if constexpr (Updating) {
        if (result.videoDecoderShape.present != decoderShape.present ||
            result.videoFrameShape.present != frameShape.present ||
            result.audioShape.has_value() != hasAudio ||
            (hasAudio && (result.audioShape->present != audioShape.present ||
                result.audioShape->externalCorrection != audioShape.externalCorrection)) ||
            (!hasAudio && !result.audio.empty()))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Composition source branch shape changed during policy rebinding"));
    } else {
        result.videoDecoderShape = decoderShape;
        result.videoFrameShape = frameShape;
        if (hasAudio) result.audioShape = audioShape;
    }
    const MediaVideoBranchConnectionPolicies videoPolicies{policies.metadata, policies.startupVideoRelease,
        policies.synchronizedVideoFrame, policies.synchronizedVideoFrame, policies.preparedVideoFrame};
    auto framePolicies = videoPolicies;
    if (!canonical) framePolicies.inputFrame = policies.preparedVideoFrame;
    const auto video = [&](const MediaVideoBranchShape& shape,
        const MediaVideoBranchConnectionPolicies& edgePolicies,
        MediaConnectionCatalog<MediaVideoBranchConnection>& connections) -> ::media::Status {
        if constexpr (Updating) {
            return MediaVideoBranchConnectionsPlanner::rebind(shape, edgePolicies, connections);
        } else {
            auto planned = MediaVideoBranchConnectionsPlanner::plan(shape, edgePolicies, *result.resource());
            if (!planned) return ::media::Status::failure(planned.error());
            connections = std::move(planned).value();
            return ::media::Status::success();
        }
    };
    if (auto status = video(decoderShape, videoPolicies, result.videoDecoder); !status) return status;
    if (auto status = video(frameShape, framePolicies, result.videoFrames); !status) return status;
    if (hasAudio) {
        if constexpr (Updating) {
            return MediaAudioBranchConnectionsPlanner::rebind(audioShape, policies, result.audio);
        } else {
            auto planned = MediaAudioBranchConnectionsPlanner::plan(audioShape, policies, *result.resource());
            if (!planned) return ::media::Status::failure(planned.error());
            result.audio = std::move(planned).value();
        }
    }
    return ::media::Status::success();
}
} // namespace
::media::Result<MediaCompositionSourceBranchPlan> MediaCompositionSourceBranchPlanner::plan(
    const MediaVideoSourceTiming& timing, bool filterActive, bool hasAudio,
    const MediaRealtimeEdgePolicySet& policies,
    std::shared_ptr<MediaPreparationMemoryResource> resource)
try {
    using R = ::media::Result<MediaCompositionSourceBranchPlan>;
    if (!resource) return R::failure(::media::ErrorInfo::invalidArgument("Source branches require a preparation resource"));
    MediaCompositionSourceBranchPlan result(std::move(resource));
    if (auto status = compileBranches<false>(timing, filterActive, hasAudio, policies, result); !status)
        return R::failure(status.error());
    return R::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionSourceBranchPlan>::failure(
        ::media::ErrorInfo::allocationFailed("composition source branch catalog"));
}
::media::Status MediaCompositionSourceBranchPlanner::rebind(
    const MediaVideoSourceTiming& timing, bool filterActive, bool hasAudio,
    const MediaRealtimeEdgePolicySet& policies, MediaCompositionSourceBranchPlan& result)
{
    return compileBranches<true>(timing, filterActive, hasAudio, policies, result);
}
} // namespace media::ffmpeg::graph
