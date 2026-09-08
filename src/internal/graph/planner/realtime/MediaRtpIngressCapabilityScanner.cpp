#include "internal/graph/planner/realtime/MediaRtpIngressCapabilityScanner.h"

#include <array>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {

enum class PlatformFamily { Windows, Linux };

::media::Result<PlatformFamily> platformFamily(
    MediaRtpIngressAdapterKind kind)
{
    switch (kind) {
    case MediaRtpIngressAdapterKind::WindowsRegisteredIo:
    case MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue:
        return ::media::Result<PlatformFamily>::success(
            PlatformFamily::Windows);
    case MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy:
    case MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages:
        return ::media::Result<PlatformFamily>::success(PlatformFamily::Linux);
    default:
        return ::media::Result<PlatformFamily>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress scan contains an unknown adapter kind"));
    }
}

std::array<MediaRtpIngressAdapterKind, 2> preference(
    PlatformFamily family) noexcept
{
    if (family == PlatformFamily::Windows) {
        return {MediaRtpIngressAdapterKind::WindowsRegisteredIo,
                MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue};
    }
    return {MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy,
            MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages};
}

const char* adapterName(MediaRtpIngressAdapterKind kind) noexcept
{
    switch (kind) {
    case MediaRtpIngressAdapterKind::WindowsRegisteredIo:
        return "windows_registered_io";
    case MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue:
        return "windows_overlapped_completion_queue";
    case MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy:
        return "linux_io_uring_zero_copy";
    case MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages:
        return "linux_receive_multiple_messages";
    default:
        return "unknown";
    }
}

} // namespace

::media::Result<MediaRtpIngressAdapterKind>
MediaRtpIngressCapabilityScanner::select(
    std::vector<MediaRtpIngressAdapterAvailability> candidates)
{
    if (candidates.empty()) {
        return ::media::Result<MediaRtpIngressAdapterKind>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP ingress capability scan produced no candidates"));
    }
    const auto firstFamily = platformFamily(candidates.front().adapterKind);
    if (!firstFamily) {
        return ::media::Result<MediaRtpIngressAdapterKind>::failure(
            firstFamily.error());
    }
    const auto orderedKinds = preference(firstFamily.value());
    std::array<const MediaRtpIngressAdapterAvailability*, 2> indexed{};
    for (const auto& candidate : candidates) {
        const auto family = platformFamily(candidate.adapterKind);
        if (!family || family.value() != firstFamily.value() ||
            candidate.available == !candidate.unavailableReason.empty()) {
            return ::media::Result<MediaRtpIngressAdapterKind>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP ingress capability scan contains inconsistent platform evidence"));
        }
        for (std::size_t index = 0; index < orderedKinds.size(); ++index) {
            if (candidate.adapterKind != orderedKinds[index]) continue;
            if (indexed[index]) {
                return ::media::Result<MediaRtpIngressAdapterKind>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "RTP ingress capability scan contains a duplicate adapter"));
            }
            indexed[index] = &candidate;
        }
    }
    if (!indexed[0] || !indexed[1]) {
        return ::media::Result<MediaRtpIngressAdapterKind>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP ingress capability scan did not report every platform candidate"));
    }
    for (const auto* candidate : indexed) {
        if (candidate->available) {
            return ::media::Result<MediaRtpIngressAdapterKind>::success(
                candidate->adapterKind);
        }
    }
    std::ostringstream message;
    message << "RTP ingress has no available platform adapter: ";
    for (std::size_t index = 0; index < indexed.size(); ++index) {
        if (index != 0) message << "; ";
        message << adapterName(indexed[index]->adapterKind) << '='
                << indexed[index]->unavailableReason;
    }
    return ::media::Result<MediaRtpIngressAdapterKind>::failure(
        ::media::ErrorInfo::unsupported(message.str()));
}

} // namespace media::ffmpeg::graph
