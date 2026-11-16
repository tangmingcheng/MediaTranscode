#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptorValidator.h"

namespace media::ffmpeg::graph {

MediaWireDatagramLayoutValidator::MediaWireDatagramLayoutValidator(
    std::uint64_t payloadBytes) noexcept
    : m_payloadBytes(payloadBytes)
{}

::media::Status MediaWireDatagramLayoutValidator::accept(
    const MediaWireDatagramLayout& descriptor)
{
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    if ((descriptor.mediaBoundary != MediaWireMediaBoundary::None &&
         descriptor.mediaBoundary != MediaWireMediaBoundary::VideoRandomAccessUnitEnd) ||
        descriptor.generation == 0 || descriptor.endpointId == 0 ||
        descriptor.payloadSize == 0 ||
        descriptor.payloadOffset != m_expectedOffset ||
        descriptor.payloadOffset > m_payloadBytes ||
        descriptor.payloadSize > m_payloadBytes - descriptor.payloadOffset ||
        descriptor.canonicalRelease < zero ||
        descriptor.canonicalDeadline < descriptor.canonicalRelease ||
        (m_generation && descriptor.generation != *m_generation) ||
        (m_previousRelease &&
         descriptor.canonicalRelease < *m_previousRelease) ||
        (m_previousDeadline &&
         descriptor.canonicalDeadline < *m_previousDeadline)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram descriptor violates payload, generation, sequence, "
            "or "
            "canonical time ordering"));
    }

    if (descriptor.protocolDeadlineObservation) {
        const auto& observation = *descriptor.protocolDeadlineObservation;
        if (observation.publication) {
            const auto& publication = *observation.publication;
            if (publication.acceptedWork.generation != descriptor.generation ||
                !publication.cursorIdentity || !publication.publicationIdentity ||
                publication.protocolBegin >= publication.protocolEnd)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Wire publication observation differs from its generation or protocol range"));
        }
        if (observation.prefixCompletionLimit <= descriptor.canonicalRelease ||
            observation.originalDeadline < observation.prefixCompletionLimit ||
            descriptor.canonicalDeadline > observation.prefixCompletionLimit)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Wire deadline exceeds its original protocol or prefix completion limit"));
    }

    m_generation = descriptor.generation;
    m_expectedOffset = descriptor.payloadOffset + descriptor.payloadSize;
    m_previousRelease = descriptor.canonicalRelease;
    m_previousDeadline = descriptor.canonicalDeadline;
    return ::media::Status::success();
}

::media::Status MediaWireDatagramLayoutValidator::finish() const
{
    if (!m_generation || m_expectedOffset != m_payloadBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram descriptors must cover their payload exactly"));
    }
    return ::media::Status::success();
}

std::uint64_t MediaWireDatagramLayoutValidator::generation() const noexcept
{
    return m_generation.value_or(0);
}

MediaWireDatagramDescriptorValidator::MediaWireDatagramDescriptorValidator(
    std::uint64_t payloadBytes) noexcept : m_layout(payloadBytes) {}

::media::Status MediaWireDatagramDescriptorValidator::accept(
    const MediaWireDatagramDescriptor& descriptor)
{
    if (m_previousSequence && descriptor.globalSequence <= *m_previousSequence) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram descriptor violates payload, generation, sequence, "
            "or canonical time ordering"));
    }
    auto accepted = m_layout.accept(descriptor);
    if (!accepted) return accepted;
    m_previousSequence = descriptor.globalSequence;
    return ::media::Status::success();
}

::media::Status MediaWireDatagramDescriptorValidator::finish() const
{
    return m_layout.finish();
}

std::uint64_t MediaWireDatagramDescriptorValidator::generation() const noexcept
{
    return m_layout.generation();
}

} // namespace media::ffmpeg::graph
