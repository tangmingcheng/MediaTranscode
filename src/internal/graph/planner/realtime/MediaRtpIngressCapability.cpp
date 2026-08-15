#include "internal/graph/planner/realtime/MediaRtpIngressCapability.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool isPowerOfTwo(std::size_t value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

bool hasMatchingContracts(const MediaRtpIngressCapabilityFacts& facts) noexcept
{
    switch (facts.adapterKind) {
    case MediaRtpIngressAdapterKind::WindowsRegisteredIo:
        return facts.storageOwnership ==
                   MediaRtpIngressStorageOwnership::RegisteredReusableArena &&
               facts.cancellationContract ==
                   MediaRtpIngressCancellationContract::CompletionQueueWake &&
               facts.completionEvidence ==
                   MediaRtpIngressCompletionEvidence::RegisteredCompletionQueue;
    case MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue:
        return facts.storageOwnership ==
                   MediaRtpIngressStorageOwnership::ReusableMessageArena &&
               facts.cancellationContract ==
                   MediaRtpIngressCancellationContract::CompletionQueueWake &&
               facts.completionEvidence ==
                   MediaRtpIngressCompletionEvidence::OverlappedCompletionPort;
    case MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy:
        return facts.storageOwnership ==
                   MediaRtpIngressStorageOwnership::RegisteredReusableArena &&
               facts.cancellationContract ==
                   MediaRtpIngressCancellationContract::DescriptorWake &&
               facts.completionEvidence ==
                   MediaRtpIngressCompletionEvidence::IoUringCompletionQueue;
    case MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages:
        return facts.storageOwnership ==
                   MediaRtpIngressStorageOwnership::ReusableMessageArena &&
               facts.cancellationContract ==
                   MediaRtpIngressCancellationContract::DescriptorWake &&
               facts.completionEvidence ==
                   MediaRtpIngressCompletionEvidence::ReceiveMultipleMessagesReturn;
    default:
        return false;
    }
}

} // namespace

MediaRtpIngressCapability::MediaRtpIngressCapability(
    MediaRtpIngressCapabilityFacts facts) noexcept
    : m_facts(facts)
{
}

::media::Result<MediaRtpIngressCapability>
MediaRtpIngressCapability::create(MediaRtpIngressCapabilityFacts facts)
{
    MediaRtpIngressCapability product(facts);
    if (auto status = product.validateProduct(); !status) {
        return ::media::Result<MediaRtpIngressCapability>::failure(
            status.error());
    }
    return ::media::Result<MediaRtpIngressCapability>::success(
        std::move(product));
}

MediaRtpIngressAdapterKind
MediaRtpIngressCapability::adapterKind() const noexcept
{
    return m_facts.adapterKind;
}

std::size_t MediaRtpIngressCapability::effectiveSocketReceiveBytes() const noexcept
{
    return m_facts.effectiveSocketReceiveBytes;
}

std::size_t MediaRtpIngressCapability::requiredBufferAlignmentBytes() const noexcept
{
    return m_facts.requiredBufferAlignmentBytes;
}

MediaRtpIngressStorageOwnership
MediaRtpIngressCapability::storageOwnership() const noexcept
{
    return m_facts.storageOwnership;
}

MediaRtpIngressCancellationContract
MediaRtpIngressCapability::cancellationContract() const noexcept
{
    return m_facts.cancellationContract;
}

MediaRtpIngressCompletionEvidence
MediaRtpIngressCapability::completionEvidence() const noexcept
{
    return m_facts.completionEvidence;
}

::media::Status MediaRtpIngressCapability::validateProduct() const
{
    if (m_facts.effectiveSocketReceiveBytes == 0 ||
        !isPowerOfTwo(m_facts.requiredBufferAlignmentBytes) ||
        !hasMatchingContracts(m_facts)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress capability requires complete matching platform facts"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
