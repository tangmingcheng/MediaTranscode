#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuilder.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaWireDatagramBatchBuilder::MediaWireDatagramBatchBuilder(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation)
{
}

::media::Result<MediaWireDatagramBatchBuilder>
MediaWireDatagramBatchBuilder::create(
    const std::string& sessionKey,
    const std::string& serviceScopeId,
    std::uint64_t generation)
{
    using Result = ::media::Result<MediaWireDatagramBatchBuilder>;
    if (sessionKey.empty() || serviceScopeId.empty() || generation == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram builder requires complete service identity"));
    }
    try {
        return Result::success(MediaWireDatagramBatchBuilder(
            sessionKey, serviceScopeId, generation));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire datagram builder identity"));
    }
}

::media::Status MediaWireDatagramBatchBuilder::append(
    std::span<const std::uint8_t> bytes,
    std::uint64_t endpointId,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline,
    std::uint64_t globalSequence,
    MediaDatagramSubmitCommitLease commitLease)
{
    if (m_finished || m_generation == 0 || endpointId == 0 ||
        bytes.empty() || canonicalRelease < MediaRunningTime::fromNanoseconds(0) ||
        canonicalDeadline < canonicalRelease || !commitLease.valid() ||
        bytes.size() >
            (std::numeric_limits<std::uint64_t>::max)() - m_payload.size()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "wire datagram builder requires complete ordered bytes, endpoint, time, and lease facts"));
    }
    const auto offset = static_cast<std::uint64_t>(m_payload.size());
    try {
        m_payload.insert(m_payload.end(), bytes.begin(), bytes.end());
        m_entries.push_back(MediaWireDatagramBatchEntry{
            MediaWireDatagramDescriptor{
                m_generation,
                endpointId,
                offset,
                static_cast<std::uint64_t>(bytes.size()),
                canonicalRelease,
                canonicalDeadline,
                globalSequence},
            std::move(commitLease)});
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "wire datagram batch builder append"));
    }
    return ::media::Status::success();
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaWireDatagramBatchBuilder::finish()
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    if (m_finished || m_payload.empty() || m_entries.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram batch builder can finish one non-empty batch"));
    }
    m_finished = true;
    return MediaWireDatagramBatchBuffer::create(
        std::move(m_sessionKey), std::move(m_serviceScopeId),
        std::move(m_payload), std::move(m_entries));
}

} // namespace media::ffmpeg::graph
