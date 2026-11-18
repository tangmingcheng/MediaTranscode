#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptorValidator.h"

#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
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
    const MediaDatagramSubmittedService& submitted) noexcept
{
    if (!m_control || !m_scheduled || count == 0 ||
        count > m_count - m_committed) {
        abandon();
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "datagram commit slice submitted prefix is invalid"));
    }
    auto committed = m_control->commitSubmittedPrefix(
        m_begin + m_committed, count, submitted);
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
    const MediaWireDatagramLayout& layout) noexcept
    : m_bytes(bytes),
      m_layout(layout)
{
}

::media::Result<MediaSharedAllocationGeometry>
MediaWireDatagramBatchBuffer::allocationGeometry()
{
    return probeMediaSharedAllocationGeometry<MediaWireDatagramBatchBuffer,
        ConstructionKey, std::shared_ptr<const MediaWireGlobalSequenceState>,
        std::uint64_t, MediaOwnedByteStorage, MediaFixedCapacityStorage<MediaWireDatagram>>();
}

MediaWireDatagramBatchBuffer::MediaWireDatagramBatchBuffer(ConstructionKey,
    std::shared_ptr<const MediaWireGlobalSequenceState> identity,
    std::uint64_t generation, MediaOwnedByteStorage payload,
    MediaFixedCapacityStorage<MediaWireDatagram> datagrams)
    : m_identity(std::move(identity)), m_generation(generation),
      m_payload(std::move(payload)), m_datagrams(std::move(datagrams))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::WireDatagramBatch);
    setDiagnosticName(MediaDiagnosticLiteral{"wire_datagram_batch"});
}

::media::Result<MediaWireDatagramBatchBuffer::Prepared>
MediaWireDatagramBatchBuffer::prepare(
    std::shared_ptr<const MediaWireGlobalSequenceState> identity,
    std::span<const MediaWireDatagramBatchInput> inputs,
    MediaOwnedByteStorage payload, MediaFixedCapacityStorage<MediaWireDatagram> datagrams,
    const MediaSharedAllocationGeometry& geometry,
    std::shared_ptr<MediaNetworkMemoryResource> resource, MediaOwnedMemoryAllocation control)
{
    using Result = ::media::Result<Prepared>;
    if (!identity || identity->sessionKey().empty() || identity->serviceScopeId().empty() ||
        !identity->generation() || inputs.empty() || payload.empty() || !resource ||
        payload.resource() != resource.get() || datagrams.resource() != resource.get() ||
        !datagrams.empty() || datagrams.capacity() != inputs.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Wire batch requires identity and exact admitted output backing"));
    }
    MediaWireDatagramLayoutValidator validator(payload.size());
    std::size_t offset = 0;
    for (const auto& input : inputs) {
        if (input.bytes.size() > payload.size() - offset)
            return Result::failure(::media::ErrorInfo::invalidArgument("Wire batch payload exceeds admitted backing"));
        const MediaWireDatagramLayout layout{identity->generation(), input.endpointId,
            static_cast<std::uint64_t>(offset), static_cast<std::uint64_t>(input.bytes.size()),
            input.canonicalRelease, input.canonicalDeadline, input.mediaBoundary,
            input.protocolDeadlineObservation};
        auto valid = validator.accept(layout);
        if (!valid) return Result::failure(valid.error());
        auto written = input.bytes.writeInto(payload.view().subspan(offset, input.bytes.size()));
        if (!written) return Result::failure(written.error());
        datagrams.push_back(MediaWireDatagram({payload.data() + offset, input.bytes.size()}, layout));
        offset += input.bytes.size();
    }
    auto complete = validator.finish();
    if (!complete) return Result::failure(complete.error());
    try {
        const auto generation = identity->generation();
        return Result::success(Prepared(std::allocate_shared<MediaWireDatagramBatchBuffer>(
            MediaSharedAllocationAllocator<MediaWireDatagramBatchBuffer>{
                MediaSharedAllocationAdmitted{std::move(resource), geometry, &control}},
            ConstructionKey{}, std::move(identity), generation, std::move(payload), std::move(datagrams))));
    } catch (const MediaSharedAllocationMismatch&) {
        return Result::failure(::media::ErrorInfo::invalidArgument("Wire batch allocation differs from planner geometry"));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Wire batch prepared control"));
    }
}
::media::Status MediaWireDatagramBatchBuffer::bindCommit(MediaDatagramCommitSlice slice)
{
    if (m_commitSlice || m_datagrams.empty() || !slice.valid() ||
        !slice.matches(m_generation, slice.m_firstGlobalSequence, m_datagrams.size()) ||
        m_datagrams.size() - 1 > (std::numeric_limits<std::uint64_t>::max)() - slice.m_firstGlobalSequence) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram batch commit slice differs from its descriptor range"));
    }
    MediaWireDatagramDescriptorValidator validator(m_payload.size());
    for (std::size_t index = 0; index < m_datagrams.size(); ++index) {
        auto& datagram = m_datagrams[index];
        const auto sequence = slice.m_firstGlobalSequence + index;
        auto valid = validator.accept(MediaWireDatagramDescriptor{datagram.m_layout, sequence});
        if (!valid) return valid;
        datagram.m_globalSequence = sequence;
    }
    auto complete = validator.finish();
    if (!complete) return complete;
    m_commitSlice.emplace(std::move(slice));
    return ::media::Status::success();
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaWireDatagramBatchBuffer::Prepared::bind(MediaDatagramCommitSlice slice) &&
{
    using Result = ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    auto buffer = std::move(m_buffer);
    if (!buffer) return Result::failure(::media::ErrorInfo::invalidArgument(
        "wire datagram batch preparation has already been consumed"));
    auto bound = buffer->bindCommit(std::move(slice));
    if (!bound) return Result::failure(bound.error());
    return Result::success(std::move(buffer));
}

const std::string& MediaWireDatagramBatchBuffer::sessionKey() const noexcept
{
    return m_identity->sessionKey();
}

const std::string& MediaWireDatagramBatchBuffer::serviceScopeId() const noexcept
{
    return m_identity->serviceScopeId();
}

std::optional<std::uint64_t>
MediaWireDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return m_payload.size();
}

} // namespace media::ffmpeg::graph
