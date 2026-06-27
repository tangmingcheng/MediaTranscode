#include "internal/graph/runtime/zerocopy/MediaZeroCopyPlanner.h"

namespace media::ffmpeg::graph {

MediaZeroCopyPlan MediaZeroCopyPlanner::plan(const MediaHardwareDescriptor& from,
                                             const MediaHardwareDescriptor& to,
                                             const MediaZeroCopyPolicy& policy)
{
    MediaZeroCopyPlan result;

    if (!policy.enabled()) {
        result.softwareFallback = true;
        result.steps.push_back({ MediaZeroCopyPlanAction::CopySoftwareFrame,
                                 MediaInteropKind::None,
                                 "zero-copy disabled" });
        return result;
    }

    if (from.frameKind == MediaHardwareFrameKind::Software &&
        to.frameKind == MediaHardwareFrameKind::Software) {
        result.zeroCopy = true;
        result.steps.push_back({ MediaZeroCopyPlanAction::KeepReference,
                                 MediaInteropKind::AVFrameRef,
                                 "software AVFrame reference can be shared" });
        return result;
    }

    const MediaInteropKind interop = chooseInterop(from, to, policy);
    if (interop != MediaInteropKind::None) {
        result.zeroCopy = true;
        result.steps.push_back({ MediaZeroCopyPlanAction::MapHardwareFrame,
                                 interop,
                                 "compatible hardware interop selected" });
        return result;
    }

    if (policy.allowSoftwareFallback) {
        result.softwareFallback = true;
        result.steps.push_back({ MediaZeroCopyPlanAction::DownloadToSoftware,
                                 MediaInteropKind::None,
                                 "hardware interop unavailable; fallback to software" });
        return result;
    }

    result.steps.push_back({ MediaZeroCopyPlanAction::Unsupported,
                             MediaInteropKind::None,
                             "zero-copy required but no compatible interop found" });
    return result;
}

MediaInteropKind MediaZeroCopyPlanner::chooseInterop(const MediaHardwareDescriptor& from,
                                                     const MediaHardwareDescriptor& to,
                                                     const MediaZeroCopyPolicy& policy) noexcept
{
    if (policy.preferredInterop != MediaInteropKind::None) {
        if (from.deviceKind == to.deviceKind && from.isHardwareBacked() && to.isHardwareBacked()) {
            return policy.preferredInterop;
        }
    }

    if (from.deviceKind == MediaHardwareDeviceKind::DRMPrime ||
        to.deviceKind == MediaHardwareDeviceKind::DRMPrime) {
        return MediaInteropKind::DRMPrime;
    }

    if (from.deviceKind == MediaHardwareDeviceKind::CUDA ||
        to.deviceKind == MediaHardwareDeviceKind::CUDA) {
        return MediaInteropKind::CUDA;
    }

    if (from.deviceKind == MediaHardwareDeviceKind::D3D11VA ||
        to.deviceKind == MediaHardwareDeviceKind::D3D11VA) {
        return MediaInteropKind::D3D11;
    }

    if (from.deviceKind == MediaHardwareDeviceKind::RKMPP ||
        to.deviceKind == MediaHardwareDeviceKind::RKMPP) {
        return MediaInteropKind::RKMPP;
    }

    return MediaInteropKind::None;
}

} // namespace media::ffmpeg::graph
