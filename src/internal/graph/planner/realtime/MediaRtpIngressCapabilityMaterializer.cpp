#include "internal/graph/planner/realtime/MediaRtpIngressCapabilityMaterializer.h"

#include "internal/graph/planner/realtime/MediaRtpIngressCapabilityScanner.h"
#include "internal/graph/planner/realtime/MediaRtpIngressPlatformCapabilityProbe.h"

#include <cstddef>
#include <limits>
#include <utility>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace media::ffmpeg::graph {

::media::Result<MediaRtpIngressCapability>
MediaRtpIngressCapabilityMaterializer::materialize(
    std::size_t effectiveSocketReceivePayloadBytes)
{
    if (effectiveSocketReceivePayloadBytes == 0) {
        return ::media::Result<MediaRtpIngressCapability>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress capability requires an effective socket receive capacity"));
    }
    auto availability = MediaRtpIngressPlatformCapabilityProbe::scan();
    if (!availability) {
        return ::media::Result<MediaRtpIngressCapability>::failure(
            availability.error());
    }
#ifdef _WIN32
    for (auto& candidate : availability.value()) {
        if (candidate.adapterKind ==
                MediaRtpIngressAdapterKind::WindowsRegisteredIo &&
            candidate.available) {
            candidate.available = false;
            candidate.unavailableReason =
                "the application transport does not expose registered socket ownership";
        }
    }
#endif
    auto selected = MediaRtpIngressCapabilityScanner::select(
        std::move(availability).value());
    if (!selected) {
        return ::media::Result<MediaRtpIngressCapability>::failure(
            selected.error());
    }
#ifdef _WIN32
    if (selected.value() !=
        MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue) {
        return ::media::Result<MediaRtpIngressCapability>::failure(
            ::media::ErrorInfo::unsupported(
                "selected Windows raw RTP ingress adapter is not implemented"));
    }
    return MediaRtpIngressCapability::create({
        selected.value(),
        effectiveSocketReceivePayloadBytes,
        static_cast<std::size_t>(
            (std::numeric_limits<unsigned long>::max)()),
        alignof(std::max_align_t),
        MediaRtpIngressStorageOwnership::ReusableMessageArena,
        MediaRtpIngressCancellationContract::CompletionQueueWake,
        MediaRtpIngressCompletionEvidence::OverlappedCompletionPort});
#else
    if (selected.value() !=
        MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages) {
        return ::media::Result<MediaRtpIngressCapability>::failure(
            ::media::ErrorInfo::unsupported(
                "selected Linux raw RTP ingress adapter is not implemented"));
    }
    const long maximumCompletions = ::sysconf(_SC_IOV_MAX);
    if (maximumCompletions <= 0) {
        return ::media::Result<MediaRtpIngressCapability>::failure(
            ::media::ErrorInfo::notInitialized(
                "Linux raw RTP ingress has no authoritative IOV capacity"));
    }
    return MediaRtpIngressCapability::create({
        selected.value(),
        effectiveSocketReceivePayloadBytes,
        static_cast<std::size_t>(maximumCompletions),
        alignof(std::max_align_t),
        MediaRtpIngressStorageOwnership::ReusableMessageArena,
        MediaRtpIngressCancellationContract::DescriptorWake,
        MediaRtpIngressCompletionEvidence::ReceiveMultipleMessagesReturn});
#endif
}

} // namespace media::ffmpeg::graph
