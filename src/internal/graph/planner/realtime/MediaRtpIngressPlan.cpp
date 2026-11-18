#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaRtpIngressPlan::MediaRtpIngressPlan(
    MediaRtpReceiveGeometry geometry, std::size_t reorderWindowPackets,
    std::int64_t maximumReorderDelayNanoseconds) noexcept
    : m_receiveGeometry(std::move(geometry)),
      m_reorderWindowPackets(reorderWindowPackets),
      m_maximumReorderDelayNanoseconds(maximumReorderDelayNanoseconds) {}

::media::Result<MediaRtpIngressPlan> MediaRtpIngressPlan::create(
    const MediaRtpReceiveGeometry& geometry,
    const MediaRtpIngressObservation& observation,
    const MediaRtpInputWaitPlan& inputWait)
{
    using Result = ::media::Result<MediaRtpIngressPlan>;
    if (auto status = geometry.validateProduct(); !status)
        return Result::failure(status.error());
    if (auto status = observation.validateProduct(); !status)
        return Result::failure(status.error());
    if (geometry.channel() != MediaRtpUdpChannel::Rtp ||
        observation.maximumDatagramBytes() > geometry.maximumDatagramBytes())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP ingress observation differs from its original receive geometry"));
    if (observation.maximumSequenceDisplacementPackets() > geometry.descriptorCapacity())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP ingress reorder observation exceeds the receive descriptor budget"));
    auto delay = inputWait.finalNanoseconds(observation.maximumInterarrivalNanoseconds());
    if (!delay) return Result::failure(delay.error());
    // Observed disorder is a minimum requirement, not a future network bound.
    // Preserve the original receive product's complete descriptor capacity.
    MediaRtpIngressPlan product(geometry, geometry.descriptorCapacity(), delay.value());
    if (auto status = product.validateProduct(); !status)
        return Result::failure(status.error());
    return Result::success(std::move(product));
}

::media::Result<MediaRtpIngressPlan> MediaRtpIngressPlan::fromFacts(
    MediaRtpIngressPlanFacts facts)
{
    auto geometry = MediaRtpReceiveGeometry::fromFacts({
        facts.adapterKind, facts.socketReceiveCapacityBytes,
        facts.maximumDatagramBytes, facts.batchByteCapacity,
        facts.descriptorCapacity, facts.requiredBufferAlignmentBytes,
        facts.storageOwnership, facts.cancellationContract,
        facts.completionEvidence, MediaRtpUdpChannel::Rtp});
    if (!geometry)
        return ::media::Result<MediaRtpIngressPlan>::failure(geometry.error());
    MediaRtpIngressPlan product(std::move(geometry).value(),
        facts.reorderWindowPackets, facts.maximumReorderDelayNanoseconds);
    if (auto status = product.validateProduct(); !status)
        return ::media::Result<MediaRtpIngressPlan>::failure(status.error());
    return ::media::Result<MediaRtpIngressPlan>::success(std::move(product));
}

const MediaRtpReceiveGeometry& MediaRtpIngressPlan::receiveGeometry() const noexcept
{ return m_receiveGeometry; }

MediaRtpIngressAdapterKind MediaRtpIngressPlan::adapterKind() const noexcept
{ return m_receiveGeometry.adapterKind(); }

std::size_t MediaRtpIngressPlan::socketReceiveCapacityBytes() const noexcept
{ return m_receiveGeometry.socketReceiveCapacityBytes(); }

std::size_t MediaRtpIngressPlan::maximumDatagramBytes() const noexcept
{ return m_receiveGeometry.maximumDatagramBytes(); }

std::size_t MediaRtpIngressPlan::batchByteCapacity() const noexcept
{ return m_receiveGeometry.batchByteCapacity(); }

std::size_t MediaRtpIngressPlan::descriptorCapacity() const noexcept
{ return m_receiveGeometry.descriptorCapacity(); }

std::size_t MediaRtpIngressPlan::requiredBufferAlignmentBytes() const noexcept
{ return m_receiveGeometry.requiredBufferAlignmentBytes(); }

MediaRtpIngressStorageOwnership MediaRtpIngressPlan::storageOwnership() const noexcept
{ return m_receiveGeometry.storageOwnership(); }

MediaRtpIngressCancellationContract MediaRtpIngressPlan::cancellationContract() const noexcept
{ return m_receiveGeometry.cancellationContract(); }

MediaRtpIngressCompletionEvidence MediaRtpIngressPlan::completionEvidence() const noexcept
{ return m_receiveGeometry.completionEvidence(); }

std::size_t MediaRtpIngressPlan::reorderWindowPackets() const noexcept
{ return m_reorderWindowPackets; }

std::int64_t MediaRtpIngressPlan::maximumReorderDelayNanoseconds() const noexcept
{ return m_maximumReorderDelayNanoseconds; }

MediaRtpIngressPlanFacts MediaRtpIngressPlan::facts() const noexcept
{
    const auto geometry = m_receiveGeometry.facts();
    return {geometry.adapterKind, geometry.socketReceiveCapacityBytes,
        geometry.maximumDatagramBytes, geometry.batchByteCapacity,
        geometry.descriptorCapacity, geometry.requiredBufferAlignmentBytes,
        m_reorderWindowPackets, m_maximumReorderDelayNanoseconds,
        geometry.storageOwnership, geometry.cancellationContract,
        geometry.completionEvidence};
}

bool MediaRtpIngressPlan::sameProduct(const MediaRtpIngressPlan& other) const noexcept
{ return facts() == other.facts(); }

::media::Status MediaRtpIngressPlan::validateProduct() const
{
    if (auto status = m_receiveGeometry.validateProduct(); !status) return status;
    if (m_receiveGeometry.channel() != MediaRtpUdpChannel::Rtp ||
        !m_reorderWindowPackets || m_maximumReorderDelayNanoseconds <= 0)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP ingress plan contains incomplete or inconsistent facts"));
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
