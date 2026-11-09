#include "internal/graph/planner/realtime/MediaRtpReceiveGeometry.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaRtpReceiveGeometry::MediaRtpReceiveGeometry(
    MediaRtpReceiveGeometryFacts facts) noexcept : m_facts(facts) {}

::media::Result<MediaRtpReceiveGeometry> MediaRtpReceiveGeometry::create(
    const MediaRtpIngressCapability& capability,
    std::size_t preparedInputByteBudget,
    std::size_t maximumDatagramBytes,
    MediaRtpUdpChannel channel)
{
    using Result = ::media::Result<MediaRtpReceiveGeometry>;
    if (auto status = capability.validateProduct(); !status)
        return Result::failure(status.error());
    const auto boundedBytes = (std::min)(
        preparedInputByteBudget, capability.effectiveSocketReceivePayloadBytes());
    if (!maximumDatagramBytes || boundedBytes < maximumDatagramBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP receive facts cannot hold a complete datagram"));
    const auto descriptors = (std::min)(
        boundedBytes / maximumDatagramBytes, capability.maximumReceiveCompletions());
    // The quotient above proves this product cannot overflow or exceed the
    // explicitly supplied byte bound. Each fixed channel owns its own arena.
    return fromFacts({capability.adapterKind(),
        capability.effectiveSocketReceivePayloadBytes(), maximumDatagramBytes,
        descriptors * maximumDatagramBytes, descriptors,
        capability.requiredBufferAlignmentBytes(), capability.storageOwnership(),
        capability.cancellationContract(), capability.completionEvidence(), channel});
}

::media::Result<MediaRtpReceiveGeometry> MediaRtpReceiveGeometry::fromFacts(
    MediaRtpReceiveGeometryFacts facts)
{
    MediaRtpReceiveGeometry product(facts);
    if (auto status = product.validateProduct(); !status)
        return ::media::Result<MediaRtpReceiveGeometry>::failure(status.error());
    return ::media::Result<MediaRtpReceiveGeometry>::success(std::move(product));
}

::media::Status MediaRtpReceiveGeometry::validateProduct() const
{
    if ((m_facts.channel != MediaRtpUdpChannel::Rtp &&
         m_facts.channel != MediaRtpUdpChannel::Rtcp) ||
        !m_facts.maximumDatagramBytes || !m_facts.descriptorCapacity ||
        m_facts.maximumDatagramBytes > m_facts.socketReceiveCapacityBytes ||
        m_facts.descriptorCapacity >
            (std::numeric_limits<std::size_t>::max)() / m_facts.maximumDatagramBytes ||
        m_facts.batchByteCapacity !=
            m_facts.descriptorCapacity * m_facts.maximumDatagramBytes ||
        m_facts.batchByteCapacity > m_facts.socketReceiveCapacityBytes)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP receive geometry contains incomplete or inconsistent facts"));
    // Reuse the capability contract's adapter/storage/cancellation/alignment
    // validation rather than maintaining a second platform contract table.
    auto capability = MediaRtpIngressCapability::create({
        m_facts.adapterKind, m_facts.socketReceiveCapacityBytes,
        m_facts.descriptorCapacity, m_facts.requiredBufferAlignmentBytes,
        m_facts.storageOwnership, m_facts.cancellationContract,
        m_facts.completionEvidence});
    return capability ? ::media::Status::success()
                      : ::media::Status::failure(capability.error());
}

MediaRtpIngressAdapterKind MediaRtpReceiveGeometry::adapterKind() const noexcept
{ return m_facts.adapterKind; }

std::size_t MediaRtpReceiveGeometry::socketReceiveCapacityBytes() const noexcept
{ return m_facts.socketReceiveCapacityBytes; }

std::size_t MediaRtpReceiveGeometry::maximumDatagramBytes() const noexcept
{ return m_facts.maximumDatagramBytes; }

std::size_t MediaRtpReceiveGeometry::batchByteCapacity() const noexcept
{ return m_facts.batchByteCapacity; }

std::size_t MediaRtpReceiveGeometry::descriptorCapacity() const noexcept
{ return m_facts.descriptorCapacity; }

std::size_t MediaRtpReceiveGeometry::requiredBufferAlignmentBytes() const noexcept
{ return m_facts.requiredBufferAlignmentBytes; }

MediaRtpIngressStorageOwnership MediaRtpReceiveGeometry::storageOwnership() const noexcept
{ return m_facts.storageOwnership; }

MediaRtpIngressCancellationContract MediaRtpReceiveGeometry::cancellationContract() const noexcept
{ return m_facts.cancellationContract; }

MediaRtpIngressCompletionEvidence MediaRtpReceiveGeometry::completionEvidence() const noexcept
{ return m_facts.completionEvidence; }

MediaRtpUdpChannel MediaRtpReceiveGeometry::channel() const noexcept
{ return m_facts.channel; }

MediaRtpReceiveGeometryFacts MediaRtpReceiveGeometry::facts() const noexcept
{ return m_facts; }

bool MediaRtpReceiveGeometry::sameProduct(const MediaRtpReceiveGeometry& other) const noexcept
{ return m_facts == other.m_facts; }

} // namespace media::ffmpeg::graph
