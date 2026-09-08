#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptorValidator.h"

namespace media::ffmpeg::graph {

MediaWireDatagramDescriptorValidator::MediaWireDatagramDescriptorValidator(
    std::uint64_t payloadBytes) noexcept
    : m_payloadBytes(payloadBytes)
{}

::media::Status MediaWireDatagramDescriptorValidator::accept(
    const MediaWireDatagramDescriptor& descriptor)
{
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    if (descriptor.generation == 0 || descriptor.endpointId == 0 ||
        descriptor.payloadSize == 0 ||
        descriptor.payloadOffset != m_expectedOffset ||
        descriptor.payloadOffset > m_payloadBytes ||
        descriptor.payloadSize > m_payloadBytes - descriptor.payloadOffset ||
        descriptor.canonicalRelease < zero ||
        descriptor.canonicalDeadline < descriptor.canonicalRelease ||
        (m_generation && descriptor.generation != *m_generation) ||
        (m_previousSequence &&
         descriptor.globalSequence <= *m_previousSequence) ||
        (m_previousRelease &&
         descriptor.canonicalRelease < *m_previousRelease) ||
        (m_previousDeadline &&
         descriptor.canonicalDeadline < *m_previousDeadline)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram descriptor violates payload, generation, sequence, "
            "or "
            "canonical time ordering"));
    }

    m_generation = descriptor.generation;
    m_expectedOffset = descriptor.payloadOffset + descriptor.payloadSize;
    m_previousSequence = descriptor.globalSequence;
    m_previousRelease = descriptor.canonicalRelease;
    m_previousDeadline = descriptor.canonicalDeadline;
    return ::media::Status::success();
}

::media::Status MediaWireDatagramDescriptorValidator::finish() const
{
    if (!m_generation || m_expectedOffset != m_payloadBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram descriptors must cover their payload exactly"));
    }
    return ::media::Status::success();
}

std::uint64_t MediaWireDatagramDescriptorValidator::generation() const noexcept
{
    return m_generation.value_or(0);
}

} // namespace media::ffmpeg::graph
