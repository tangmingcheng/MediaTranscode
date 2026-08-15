#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"

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
          capability.effectiveSocketReceiveBytes()),
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
    if (preparedInputByteBudget == 0 ||
        observation.maximumDatagramBytes() >
            capability.effectiveSocketReceiveBytes() ||
        observation.maximumDatagramsPerReadiness() >
            capability.maximumReceiveDescriptors() ||
        observation.maximumDatagramsPerReadiness() >
            (std::numeric_limits<std::size_t>::max)() /
                observation.maximumDatagramBytes()) {
        return ::media::Result<MediaRtpIngressPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress facts cannot form a bounded receive product"));
    }
    const std::size_t batchByteCapacity =
        observation.maximumDatagramsPerReadiness() *
        observation.maximumDatagramBytes();
    if (batchByteCapacity > preparedInputByteBudget ||
        observation.maximumSequenceDisplacementPackets() ==
            (std::numeric_limits<std::size_t>::max)()) {
        return ::media::Result<MediaRtpIngressPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress prepared budget cannot contain observed batch and reorder facts"));
    }
    MediaRtpIngressPlan product(
        capability,
        observation.maximumDatagramBytes(),
        batchByteCapacity,
        observation.maximumDatagramsPerReadiness(),
        observation.maximumSequenceDisplacementPackets() + 1,
        observation.maximumArrivalVariationNanoseconds());
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

::media::Status MediaRtpIngressPlan::validateProduct() const
{
    if (m_socketReceiveCapacityBytes == 0 ||
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
