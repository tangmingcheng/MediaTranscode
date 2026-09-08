#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptorValidator.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaDatagramCommitTransaction::MediaDatagramCommitTransaction(
    MediaDatagramCommitTransaction&& other) noexcept
    : m_generation(other.m_generation),
      m_firstGlobalSequence(other.m_firstGlobalSequence),
      m_size(other.m_size),
      m_nextSlice(other.m_nextSlice),
      m_control(std::move(other.m_control))
{
}

MediaDatagramCommitTransaction& MediaDatagramCommitTransaction::operator=(
    MediaDatagramCommitTransaction&& other) noexcept
{
    if (this == &other) return *this;
    abandonUnsliced();
    m_generation = other.m_generation;
    m_firstGlobalSequence = other.m_firstGlobalSequence;
    m_size = other.m_size;
    m_nextSlice = other.m_nextSlice;
    m_control = std::move(other.m_control);
    return *this;
}

MediaDatagramCommitTransaction::~MediaDatagramCommitTransaction() noexcept
{
    abandonUnsliced();
}

::media::Result<std::uint64_t> MediaDatagramCommitTransaction::sequence(
    std::size_t index) const noexcept
{
    if (!m_control || index >= m_size) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "datagram commit transaction sequence index is invalid"));
    }
    return ::media::Result<std::uint64_t>::success(
        m_firstGlobalSequence + static_cast<std::uint64_t>(index));
}

::media::Result<MediaDatagramCommitSlice>
MediaDatagramCommitTransaction::takeNextSlice(std::size_t count) noexcept
{
    using Result = ::media::Result<MediaDatagramCommitSlice>;
    if (!m_control || count == 0 || count > m_size - m_nextSlice) {
        abandonUnsliced();
        return Result::failure(::media::ErrorInfo::internalError(
            "datagram commit transaction slice is empty or outside its range"));
    }
    const auto first = m_firstGlobalSequence +
        static_cast<std::uint64_t>(m_nextSlice);
    MediaDatagramCommitSlice slice(
        m_control, m_generation, first, m_nextSlice, count);
    m_nextSlice += count;
    return Result::success(std::move(slice));
}

void MediaDatagramCommitTransaction::abandonUnsliced() noexcept
{
    if (m_control && m_nextSlice != m_size) m_control->abandon();
    m_control.reset();
}

MediaDatagramCommitSlice::MediaDatagramCommitSlice(
    std::shared_ptr<MediaDatagramCommitTransaction::Concept> control,
    std::uint64_t generation,
    std::uint64_t firstGlobalSequence,
    std::size_t begin,
    std::size_t count) noexcept
    : m_control(std::move(control)),
      m_generation(generation),
      m_firstGlobalSequence(firstGlobalSequence),
      m_begin(begin),
      m_count(count)
{
}

MediaDatagramCommitSlice::MediaDatagramCommitSlice(
    MediaDatagramCommitSlice&& other) noexcept
    : m_control(std::move(other.m_control)),
      m_generation(other.m_generation),
      m_firstGlobalSequence(other.m_firstGlobalSequence),
      m_begin(other.m_begin),
      m_count(other.m_count),
      m_committed(other.m_committed),
      m_scheduled(other.m_scheduled)
{
}

MediaDatagramCommitSlice& MediaDatagramCommitSlice::operator=(
    MediaDatagramCommitSlice&& other) noexcept
{
    if (this == &other) return *this;
    abandon();
    m_control = std::move(other.m_control);
    m_generation = other.m_generation;
    m_firstGlobalSequence = other.m_firstGlobalSequence;
    m_begin = other.m_begin;
    m_count = other.m_count;
    m_committed = other.m_committed;
    m_scheduled = other.m_scheduled;
    return *this;
}

MediaDatagramCommitSlice::~MediaDatagramCommitSlice() noexcept
{
    abandon();
}

bool MediaDatagramCommitSlice::matches(
    std::uint64_t generation,
    std::uint64_t firstGlobalSequence,
    std::size_t count) const noexcept
{
    return m_control && m_generation == generation &&
           m_firstGlobalSequence == firstGlobalSequence && m_count == count;
}

::media::Status MediaDatagramCommitSlice::scheduleAll(
    MediaRunningTime now) noexcept
{
    if (!m_control || m_scheduled) {
        abandon();
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "datagram commit slice cannot be scheduled twice or after move"));
    }
    auto scheduled = m_control->markScheduledPrefix(m_begin, m_count, now);
    if (!scheduled) {
        abandon();
        return scheduled;
    }
    m_scheduled = true;
    return ::media::Status::success();
}

::media::Status MediaDatagramCommitSlice::commitSubmittedPrefix(
    std::size_t count,
    MediaRunningTime now) noexcept
{
    if (!m_control || !m_scheduled || count == 0 ||
        count > m_count - m_committed) {
        abandon();
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "datagram commit slice submitted prefix is invalid"));
    }
    auto committed = m_control->commitSubmittedPrefix(
        m_begin + m_committed, count, now);
    if (!committed) {
        abandon();
        return committed;
    }
    m_committed += count;
    if (m_committed == m_count) m_control.reset();
    return ::media::Status::success();
}

void MediaDatagramCommitSlice::abandon() noexcept
{
    if (m_control && m_committed != m_count) m_control->abandon();
    m_control.reset();
}

MediaWireDatagram::MediaWireDatagram(
    std::span<const std::uint8_t> bytes,
    const MediaWireDatagramDescriptor& descriptor) noexcept
    : m_bytes(bytes),
      m_descriptor(descriptor)
{
}

MediaWireDatagramBatchBuffer::MediaWireDatagramBatchBuffer(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    std::vector<std::uint8_t> payload,
    std::vector<MediaWireDatagram> datagrams,
    MediaDatagramCommitSlice commitSlice) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation),
      m_payload(std::move(payload)),
      m_datagrams(std::move(datagrams)),
      m_commitSlice(std::move(commitSlice))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::WireDatagramBatch);
    setDiagnosticName("wire_datagram_batch");
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaWireDatagramBatchBuffer::create(
    std::string sessionKey,
    std::string serviceScopeId,
    std::vector<std::uint8_t> payload,
    std::vector<MediaWireDatagramBatchEntry> entries,
    MediaDatagramCommitSlice commitSlice)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    if (sessionKey.empty() || serviceScopeId.empty() || payload.empty() ||
        entries.empty() || !commitSlice.valid()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram batch requires service identity, payload, and entries"));
    }
    std::vector<MediaWireDatagram> datagrams;
    try {
        datagrams.reserve(entries.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire datagram entries"));
    }

    MediaWireDatagramDescriptorValidator validator(
        static_cast<std::uint64_t>(payload.size()));
    for (auto& entry : entries) {
        const auto& descriptor = entry.descriptor;
        auto validDescriptor = validator.accept(descriptor);
        if (!validDescriptor) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "wire datagram batch violates its shared descriptor or lease ownership"));
        }
        try {
            datagrams.push_back(MediaWireDatagram(
                std::span<const std::uint8_t>(
                    payload.data() + descriptor.payloadOffset,
                    static_cast<std::size_t>(descriptor.payloadSize)),
                descriptor));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "wire datagram batch entries"));
        }
    }
    auto complete = validator.finish();
    if (!complete) return Result::failure(complete.error());
    if (!commitSlice.matches(
            validator.generation(), entries.front().descriptor.globalSequence,
            entries.size())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram batch commit slice differs from its descriptor range"));
    }
    try {
        return Result::success(std::shared_ptr<MediaWireDatagramBatchBuffer>(
            new MediaWireDatagramBatchBuffer(
                std::move(sessionKey), std::move(serviceScopeId),
                validator.generation(), std::move(payload),
                std::move(datagrams), std::move(commitSlice))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaWireDatagramBatchBuffer"));
    }
}

std::optional<std::uint64_t>
MediaWireDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return m_payload.size();
}

} // namespace media::ffmpeg::graph
