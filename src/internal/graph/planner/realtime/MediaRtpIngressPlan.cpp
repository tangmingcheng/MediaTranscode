#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaRtpIngressPlan::MediaRtpIngressPlan(
    const MediaRtpIngressCapability& capability,
    std::size_t maximumDatagramBytes,
    std::size_t batchByteCapacity,
    std::size_t descriptorCapacity,
    std::size_t reorderWindowPackets,
    std::int64_t maximumReorderDelayNanoseconds) noexcept
    : m_adapterKind(capability.adapterKind()),
      m_socketReceiveCapacityBytes(
          capability.effectiveSocketReceivePayloadBytes()),
      m_maximumDatagramBytes(maximumDatagramBytes),
      m_batchByteCapacity(batchByteCapacity),
      m_descriptorCapacity(descriptorCapacity),
      m_requiredBufferAlignmentBytes(
          capability.requiredBufferAlignmentBytes()),
      m_reorderWindowPackets(reorderWindowPackets),
      m_maximumReorderDelayNanoseconds(maximumReorderDelayNanoseconds),
      m_storageOwnership(capability.storageOwnership()),
      m_cancellationContract(capability.cancellationContract()),
      m_completionEvidence(capability.completionEvidence())
{
}

MediaRtpIngressPlan::MediaRtpIngressPlan(
    MediaRtpIngressPlanFacts facts) noexcept
    : m_adapterKind(facts.adapterKind),
      m_socketReceiveCapacityBytes(facts.socketReceiveCapacityBytes),
      m_maximumDatagramBytes(facts.maximumDatagramBytes),
      m_batchByteCapacity(facts.batchByteCapacity),
      m_descriptorCapacity(facts.descriptorCapacity),
      m_requiredBufferAlignmentBytes(facts.requiredBufferAlignmentBytes),
      m_reorderWindowPackets(facts.reorderWindowPackets),
      m_maximumReorderDelayNanoseconds(
          facts.maximumReorderDelayNanoseconds),
      m_storageOwnership(facts.storageOwnership),
      m_cancellationContract(facts.cancellationContract),
      m_completionEvidence(facts.completionEvidence)
{
}

::media::Result<MediaRtpIngressPlan> MediaRtpIngressPlan::create(
    const MediaRtpIngressCapability& capability,
    const MediaRtpIngressObservation& observation,
    std::size_t preparedInputByteBudget)
{
    if (auto status = capability.validateProduct(); !status) {
        return ::media::Result<MediaRtpIngressPlan>::failure(status.error());
    }
    if (auto status = observation.validateProduct(); !status) {
        return ::media::Result<MediaRtpIngressPlan>::failure(status.error());
    }
    const std::size_t boundedReceiveBytes = (std::min)(
        preparedInputByteBudget,
        capability.effectiveSocketReceivePayloadBytes());
    if (boundedReceiveBytes < observation.maximumDatagramBytes()) {
        return ::media::Result<MediaRtpIngressPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress facts cannot form a bounded receive product"));
    }
    const std::size_t descriptorCapacity = (std::min)(
        boundedReceiveBytes / observation.maximumDatagramBytes(),
        capability.maximumReceiveCompletions());
    if (capability.adapterKind() ==
            MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue &&
        descriptorCapacity < 2) {
        return ::media::Result<MediaRtpIngressPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Windows RTP ingress requires receive descriptors for both RTP and RTCP sockets"));
    }
    const std::size_t batchByteCapacity =
        descriptorCapacity * observation.maximumDatagramBytes();
    if (observation.maximumSequenceDisplacementPackets() ==
            (std::numeric_limits<std::size_t>::max)()) {
        return ::media::Result<MediaRtpIngressPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress reorder observation exceeds planner range"));
    }
    MediaRtpIngressPlan product(
        capability,
        observation.maximumDatagramBytes(),
        batchByteCapacity,
        descriptorCapacity,
        observation.maximumSequenceDisplacementPackets() + 1,
        observation.maximumInterarrivalNanoseconds());
    if (auto status = product.validateProduct(); !status) {
        return ::media::Result<MediaRtpIngressPlan>::failure(status.error());
    }
    return ::media::Result<MediaRtpIngressPlan>::success(std::move(product));
}

::media::Result<MediaRtpIngressPlan> MediaRtpIngressPlan::fromFacts(
    MediaRtpIngressPlanFacts facts)
{
    MediaRtpIngressPlan product(std::move(facts));
    if (auto status = product.validateProduct(); !status) {
        return ::media::Result<MediaRtpIngressPlan>::failure(status.error());
    }
    return ::media::Result<MediaRtpIngressPlan>::success(std::move(product));
}

MediaRtpIngressAdapterKind MediaRtpIngressPlan::adapterKind() const noexcept
{
    return m_adapterKind;
}

std::size_t MediaRtpIngressPlan::socketReceiveCapacityBytes() const noexcept
{
    return m_socketReceiveCapacityBytes;
}

std::size_t MediaRtpIngressPlan::maximumDatagramBytes() const noexcept
{
    return m_maximumDatagramBytes;
}

std::size_t MediaRtpIngressPlan::batchByteCapacity() const noexcept
{
    return m_batchByteCapacity;
}

std::size_t MediaRtpIngressPlan::descriptorCapacity() const noexcept
{
    return m_descriptorCapacity;
}

std::size_t MediaRtpIngressPlan::requiredBufferAlignmentBytes() const noexcept
{
    return m_requiredBufferAlignmentBytes;
}

std::size_t MediaRtpIngressPlan::reorderWindowPackets() const noexcept
{
    return m_reorderWindowPackets;
}

std::int64_t
MediaRtpIngressPlan::maximumReorderDelayNanoseconds() const noexcept
{
    return m_maximumReorderDelayNanoseconds;
}

MediaRtpIngressStorageOwnership
MediaRtpIngressPlan::storageOwnership() const noexcept
{
    return m_storageOwnership;
}

MediaRtpIngressCancellationContract
MediaRtpIngressPlan::cancellationContract() const noexcept
{
    return m_cancellationContract;
}

MediaRtpIngressCompletionEvidence
MediaRtpIngressPlan::completionEvidence() const noexcept
{
    return m_completionEvidence;
}

MediaRtpIngressPlanFacts MediaRtpIngressPlan::facts() const noexcept
{
    return {m_adapterKind,
            m_socketReceiveCapacityBytes,
            m_maximumDatagramBytes,
            m_batchByteCapacity,
            m_descriptorCapacity,
            m_requiredBufferAlignmentBytes,
            m_reorderWindowPackets,
            m_maximumReorderDelayNanoseconds,
            m_storageOwnership,
            m_cancellationContract,
            m_completionEvidence};
}

bool MediaRtpIngressPlan::sameProduct(
    const MediaRtpIngressPlan& other) const noexcept
{
    return facts() == other.facts();
}

::media::Status MediaRtpIngressPlan::validateProduct() const
{
    const bool knownAdapter =
        m_adapterKind == MediaRtpIngressAdapterKind::WindowsRegisteredIo ||
        m_adapterKind == MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue ||
        m_adapterKind == MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy ||
        m_adapterKind == MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages;
    const bool knownStorage =
        m_storageOwnership == MediaRtpIngressStorageOwnership::RegisteredReusableArena ||
        m_storageOwnership == MediaRtpIngressStorageOwnership::ReusableMessageArena;
    const bool knownCancellation =
        m_cancellationContract == MediaRtpIngressCancellationContract::CompletionQueueWake ||
        m_cancellationContract == MediaRtpIngressCancellationContract::DescriptorWake;
    const bool knownCompletion =
        m_completionEvidence == MediaRtpIngressCompletionEvidence::RegisteredCompletionQueue ||
        m_completionEvidence == MediaRtpIngressCompletionEvidence::OverlappedCompletionPort ||
        m_completionEvidence == MediaRtpIngressCompletionEvidence::IoUringCompletionQueue ||
        m_completionEvidence == MediaRtpIngressCompletionEvidence::ReceiveMultipleMessagesReturn;
    if (!knownAdapter || !knownStorage || !knownCancellation ||
        !knownCompletion || m_socketReceiveCapacityBytes == 0 ||
        m_maximumDatagramBytes == 0 ||
        m_maximumDatagramBytes > m_socketReceiveCapacityBytes ||
        m_descriptorCapacity == 0 ||
        m_descriptorCapacity >
            (std::numeric_limits<std::size_t>::max)() /
                m_maximumDatagramBytes ||
        m_batchByteCapacity !=
            m_descriptorCapacity * m_maximumDatagramBytes ||
        m_requiredBufferAlignmentBytes == 0 ||
        m_reorderWindowPackets == 0 ||
        m_maximumReorderDelayNanoseconds <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress plan contains incomplete or inconsistent facts"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
