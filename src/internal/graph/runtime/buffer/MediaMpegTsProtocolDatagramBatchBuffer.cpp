#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include "internal/graph/diagnostics/MediaAccessUnitProcessingTrace.h"

#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

#include <algorithm>
#include <new>
#include <mutex>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaMpegTsProtocolSeriesState final {
    MediaMpegTsProtocolSeriesState() noexcept = default;
    std::mutex mutex;
    // Declared before transaction so the borrowed cursor outlives its model.
    std::optional<MediaTsPacketCursor> cursor;
    std::optional<MediaProtocolDatagramCommitTransaction> transaction;
    std::weak_ptr<MediaNodeWakeup> commitWaiter;
    std::optional<std::int64_t> protocolCommitCompletedSteadyNs;
    std::size_t reservationBegin = 0;
    std::size_t reservationEnd = 0;
    std::optional<MediaTsPacketReservationIdentity> reservationIdentity;
    struct WireRange final {
        MediaTsPacketReservationIdentity protocolIdentity;
        std::uint64_t identity;
        std::uint64_t firstSequence;
        std::uint64_t firstMediaSequence;
        std::uint64_t lastSequence;
        std::size_t protocolBegin;
        std::size_t protocolEnd;
    };
    std::shared_ptr<MediaWireGlobalSequenceState> wireOwner;
    std::optional<WireRange> wireRange;
    bool failed = false;
    MediaFixedCapacityStorage<MediaMpegTsProtocolDatagram> datagrams;
    std::shared_ptr<MediaNetworkMemoryResource> resource;
    MediaSharedAllocationGeometry batchOwnerGeometry;
    std::size_t maximumPacketsPerDatagram = 0;
    bool matchesReservation(const MediaTsPacketReservationIdentity& identity,
        std::size_t begin, std::size_t end) const noexcept
    { return reservationIdentity == identity && reservationBegin == begin && reservationEnd == end; }
    void abandonReservation(const MediaTsPacketReservationIdentity& identity,
        std::size_t begin, std::size_t end) noexcept
    {
        failed = true;
        if (matchesReservation(identity, begin, end)) transaction->abandon();
        else cursor->poison();
    }
    std::shared_ptr<MediaNodeWakeup> takeCommitWaiter() noexcept
    {
        auto waiter = commitWaiter.lock();
        commitWaiter.reset();
        return waiter;
    }
};

namespace {

class MediaTsProtocolCommitPrefix final {
public:
    MediaTsProtocolCommitPrefix(std::shared_ptr<MediaMpegTsProtocolSeriesState> state,
        std::size_t begin, std::size_t count,
        std::size_t reservationBegin, std::size_t reservationEnd,
        MediaTsPacketReservationIdentity reservationIdentity) noexcept
        : m_state(std::move(state)), m_begin(begin), m_count(count),
          m_reservationBegin(reservationBegin), m_reservationEnd(reservationEnd),
          m_reservationIdentity(reservationIdentity) {}
    MediaTsProtocolCommitPrefix(MediaTsProtocolCommitPrefix&&) noexcept = default;
    MediaTsProtocolCommitPrefix(const MediaTsProtocolCommitPrefix&) = delete;
    ~MediaTsProtocolCommitPrefix() noexcept
    {
        if (m_state && m_committed != m_count) {
            std::unique_lock lock(m_state->mutex);
            m_state->abandonReservation(m_reservationIdentity, m_reservationBegin, m_reservationEnd);
            auto waiter = m_state->takeCommitWaiter();
            lock.unlock();
            if (waiter) waiter->notify();
        }
    }
    std::size_t size() const noexcept { return m_count; }
    ::media::Status commitNextPrefix(std::size_t count) noexcept
    {
        std::unique_lock lock(m_state->mutex);
        if (m_state->failed ||
            !m_state->matchesReservation(m_reservationIdentity, m_reservationBegin, m_reservationEnd) ||
            count == 0 || count > m_count - m_committed ||
            m_state->transaction->committed() != m_begin - m_reservationBegin + m_committed) {
            m_state->abandonReservation(m_reservationIdentity, m_reservationBegin, m_reservationEnd);
            auto waiter = m_state->takeCommitWaiter();
            lock.unlock();
            if (waiter) waiter->notify();
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS protocol prefix commit violates order or bounds"));
        }
        auto status = m_state->transaction->commitNextPrefix(count);
        if (status) m_committed += count;
        else m_state->failed = true;
        if (status && m_state->transaction->committed() == m_state->transaction->size())
            m_state->protocolCommitCompletedSteadyNs = mediaAccessUnitObservationNow();
        auto waiter = !status || m_state->transaction->committed() == m_state->transaction->size()
            ? m_state->takeCommitWaiter() : std::shared_ptr<MediaNodeWakeup>{};
        lock.unlock();
        if (waiter) waiter->notify();
        return status;
    }
private:
    std::shared_ptr<MediaMpegTsProtocolSeriesState> m_state;
    std::size_t m_begin;
    std::size_t m_count;
    std::size_t m_reservationBegin;
    std::size_t m_reservationEnd;
    MediaTsPacketReservationIdentity m_reservationIdentity;
    std::size_t m_committed = 0;
};

class MediaTsProtocolCommitReservation final {
public:
    MediaTsProtocolCommitReservation(
        MediaTsPacketCursor& cursor,
        MediaTsPacketCommitToken token,
        std::size_t entryCount) noexcept
        : m_cursor(&cursor),
          m_token(std::move(token)),
          m_entryCount(entryCount)
    {
    }

    MediaTsProtocolCommitReservation(
        MediaTsProtocolCommitReservation&& other) noexcept
        : m_cursor(other.m_cursor),
          m_token(std::move(other.m_token)),
          m_entryCount(other.m_entryCount),
          m_nextEntry(other.m_nextEntry)
    {
        other.m_nextEntry = other.m_entryCount;
    }
    MediaTsProtocolCommitReservation& operator=(
        MediaTsProtocolCommitReservation&&) = delete;

    ~MediaTsProtocolCommitReservation() noexcept
    {
        if (m_nextEntry != m_entryCount) m_cursor->poison();
    }

    std::size_t size() const noexcept { return m_entryCount; }

    ::media::Status commitNextPrefix(std::size_t count) noexcept
    {
        if (count > m_entryCount - m_nextEntry) {
            m_cursor->poison();
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS protocol commit prefix exceeds its transaction"));
        }
        m_nextEntry += count;
        if (m_nextEntry != m_entryCount) return ::media::Status::success();
        auto committed = m_cursor->commit(std::move(m_token));
        if (!committed) m_cursor->poison();
        return committed;
    }

private:
    // The containing series owns both cursor and transaction; never escape
    // this model without the series owner retained by its prefix.
    MediaTsPacketCursor* m_cursor;
    MediaTsPacketCommitToken m_token;
    std::size_t m_entryCount;
    std::size_t m_nextEntry = 0;
};

} // namespace

MediaMpegTsProtocolDatagram::MediaMpegTsProtocolDatagram(
    std::span<const std::uint8_t> bytes,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline,
    MediaRunningTime completionLimit,
    MediaWireMediaBoundary mediaBoundary) noexcept
    : m_bytes(bytes),
      m_presentationOnMaster(presentationOnMaster),
      m_canonicalRelease(canonicalRelease),
      m_canonicalDeadline(canonicalDeadline),
      m_completionLimit(completionLimit),
      m_mediaBoundary(mediaBoundary)
{
}

MediaMpegTsProtocolDatagramBatchBuffer::
MediaMpegTsProtocolDatagramBatchBuffer(ConstructionKey, std::uint64_t generation) noexcept
    : m_generation(generation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::MpegTsProtocolDatagramBatch);
    setDiagnosticName(MediaDiagnosticLiteral{"mpegts_protocol_datagram_batch"});
}

::media::Result<MediaTsProtocolSeriesStorageGeometry>
MediaMpegTsProtocolDatagramBatchBuffer::seriesStorageGeometry()
{
    using Result = ::media::Result<MediaTsProtocolSeriesStorageGeometry>;
    auto batch = probeMediaSharedAllocationGeometry<MediaMpegTsProtocolDatagramBatchBuffer,
        ConstructionKey, std::uint64_t>();
    if (!batch) return Result::failure(batch.error());
    auto commit = probeMediaSharedAllocationGeometry<MediaMpegTsProtocolSeriesState>();
    if (!commit) return Result::failure(commit.error());
    return Result::success({batch.value(), commit.value(),
        MediaProtocolDatagramCommitTransaction::allocationGeometry<MediaTsProtocolCommitReservation>()});
}

::media::Result<std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>
MediaMpegTsProtocolDatagramBatchBuffer::create(
    std::uint64_t generation,
    MediaTsPacketCursor cursor,
    std::uint16_t maximumPacketsPerDatagram,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline,
    MediaWireMediaBoundary finalBoundary,
    std::size_t maximumReservationDatagrams,
    MediaRunningTime completionLimit,
    const MediaTsAcceptedWorkObservation& acceptedWork)
{
    using Result = ::media::Result<
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>;
    if (generation == 0 || maximumPacketsPerDatagram < 1 || maximumReservationDatagrams == 0 ||
        presentationOnMaster < MediaRunningTime::fromNanoseconds(0) ||
        canonicalRelease < MediaRunningTime::fromNanoseconds(0) ||
        completionLimit <= canonicalRelease || completionLimit > canonicalDeadline) {
        cursor.poison();
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS protocol batch requires generation, packet geometry, and canonical timing"));
    }
    auto backing = cursor.remainingPackets();
    if (!backing) return Result::failure(backing.error());
    const auto packets = backing.value();
    const auto packetCount = packets.size();
    const auto entryCount = packetCount / maximumPacketsPerDatagram +
        (packetCount % maximumPacketsPerDatagram != 0);
    const auto reservedEntries = (std::min)(entryCount, maximumReservationDatagrams);
    const auto reservedPackets = reservedEntries == entryCount ? packetCount
        : reservedEntries * maximumPacketsPerDatagram;
    const auto& plan = cursor.storagePlan();
    if (!entryCount || entryCount > plan.maximumDatagrams)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS descriptors exceed the final series storage contract"));
    auto& storage = cursor.seriesStorage();
    const auto resource = cursor.storageResource();
    auto descriptors = MediaFixedCapacityStorage<MediaMpegTsProtocolDatagram>::fromAllocation(
        storage.take(MediaTsSeriesStorageRole::Descriptors));
    if (!descriptors) return Result::failure(descriptors.error());
    if (descriptors.value().capacity() < entryCount)
        return Result::failure(::media::ErrorInfo::invalidArgument("TS prepared descriptor backing is too small"));
    auto batchAllocation = storage.take(MediaTsSeriesStorageRole::BatchOwner);
    auto commitAllocation = storage.take(MediaTsSeriesStorageRole::CommitOwner);
    auto modelAllocation = storage.take(MediaTsSeriesStorageRole::CommitModel);
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> output;
    try {
        output = std::allocate_shared<MediaMpegTsProtocolDatagramBatchBuffer>(
            MediaSharedAllocationAllocator<MediaMpegTsProtocolDatagramBatchBuffer>{MediaSharedAllocationAdmitted{
                resource, plan.protocolGeometry.batchOwner, &batchAllocation}}, ConstructionKey{}, generation);
        output->m_series = std::allocate_shared<MediaMpegTsProtocolSeriesState>(
            MediaSharedAllocationAllocator<MediaMpegTsProtocolSeriesState>{MediaSharedAllocationAdmitted{
                resource, plan.protocolGeometry.commitOwner, &commitAllocation}});
        output->m_series->datagrams = std::move(descriptors).value();
        output->m_series->resource = resource;
        output->m_series->batchOwnerGeometry = plan.protocolGeometry.batchOwner;
        output->m_series->maximumPacketsPerDatagram = maximumPacketsPerDatagram;
        output->m_endDatagram = entryCount;
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("TS prepared series owner construction"));
    }
    const auto payload = std::as_bytes(packets);
    output->m_payloadBytes = static_cast<std::uint64_t>(payload.size());
    std::size_t packetOffset = 0;
    for (std::size_t index = 0; index < entryCount; ++index) {
        const auto count = (std::min)(static_cast<std::size_t>(maximumPacketsPerDatagram),
            packets.size() - packetOffset);
        output->m_series->datagrams.push_back(MediaMpegTsProtocolDatagram(
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(
                payload.data() + packetOffset * std::size_t{188}), count * std::size_t{188}),
            presentationOnMaster, canonicalRelease, canonicalDeadline, completionLimit,
            index + 1 == entryCount ? finalBoundary : MediaWireMediaBoundary::None));
        packetOffset += count;
    }
    output->m_series->cursor.emplace(std::move(cursor));
    // Backing and descriptors exist before continuity/revision publication.
    auto prepared = output->m_series->cursor->prepareSeries(MediaTsPacketReservationWork{
        generation, canonicalRelease, canonicalDeadline, completionLimit,
        0, reservedEntries, reservedPackets, maximumPacketsPerDatagram});
    if (!prepared) return Result::failure(prepared.error());
    auto token = prepared.value().takeCommitToken();
    const auto identity = token.reservationIdentity();
    if (!identity) {
        output->m_series->cursor->poison();
        return Result::failure(::media::ErrorInfo::internalError(
            "MPEG-TS prepared series has no publication identity"));
    }
    auto transaction = MediaProtocolDatagramCommitTransaction::bindPrepared(
        MediaTsProtocolCommitReservation(*output->m_series->cursor, std::move(token), reservedEntries),
        std::move(modelAllocation));
    if (!transaction) return Result::failure(transaction.error());
    output->m_series->transaction.emplace(std::move(transaction).value());
    output->m_series->reservationEnd = reservedEntries;
    output->m_series->reservationIdentity = identity;
    output->m_publicationObservation = MediaTsPublicationObservation{acceptedWork,
        identity->cursorIdentity, identity->cursorRevision, identity->publicationIdentity, 0, reservedEntries, std::nullopt};
    return Result::success(std::move(output));
}

::media::Status MediaMpegTsProtocolDatagramBatchBuffer::waitForCommit(
    const std::shared_ptr<MediaNodeWakeup>& producerWakeup,
    std::optional<std::int64_t>& protocolCommitCompletedSteadyNs) const
{
    protocolCommitCompletedSteadyNs.reset();
    if (!m_series || !producerWakeup)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS commit wait requires a series and its producer wakeup"));
    std::lock_guard lock(m_series->mutex);
    if (m_series->failed || !m_series->transaction)
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MPEG-TS series has no commit reservation"));
    if (m_series->transaction->committed() == m_series->transaction->size()) {
        protocolCommitCompletedSteadyNs = m_series->protocolCommitCompletedSteadyNs;
        return ::media::Status::success();
    }
    if (!m_series->transaction->valid())
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "MPEG-TS series commit reservation was abandoned"));
    if (auto waiter = m_series->commitWaiter.lock(); waiter && waiter != producerWakeup)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS series commit wait belongs to another producer"));
    m_series->commitWaiter = producerWakeup;
    return ::media::Status::failure(::media::ErrorInfo::wouldBlock(
        "MPEG-TS producer is waiting for its reserved prefix to commit"));
}

::media::Status MediaMpegTsProtocolDatagramBatchBuffer::reserveNextPrefix(
    std::size_t count, const MediaTsPrefixPublicationWindow& window,
    MediaOwnedMemoryAllocation allocation)
{
    if (!m_series)
        return ::media::Status::failure(::media::ErrorInfo::notInitialized("MPEG-TS series is absent"));
    std::lock_guard lock(m_series->mutex);
    const auto geometry = MediaProtocolDatagramCommitTransaction::allocationGeometry<MediaTsProtocolCommitReservation>();
    if (m_series->failed || !m_series->transaction || count == 0 ||
        m_materializedDatagrams != m_series->reservationEnd ||
        count > m_endDatagram - m_materializedDatagrams ||
        window.release.nanoseconds() < 0 || window.release >= window.completionLimit ||
        window.completionLimit > window.accessUnitDeadline ||
        !allocation || allocation.resource() != m_series->resource.get() ||
        allocation.bytes() != geometry.bytes || allocation.alignment() != geometry.alignment)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS next prefix requires its exact range and admitted commit model"));
    if (m_series->transaction->committed() != m_series->transaction->size())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS next prefix requires the previous commit to finish"));
    std::size_t packetCount = 0;
    auto next = m_series->datagrams.view().subspan(m_materializedDatagrams, count);
    if (window.release < m_series->datagrams.view()[m_series->reservationEnd - 1].canonicalRelease())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS prefix release would regress across reservations"));
    for (const auto& datagram : next) {
        if (window.accessUnitDeadline != datagram.canonicalDeadline() ||
            window.release < datagram.canonicalRelease())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS prefix window changed its original protocol timing"));
        const auto packets = datagram.bytes().size() / std::size_t{188};
        if (datagram.bytes().size() % std::size_t{188} != 0 || !packets ||
            packets > m_series->maximumPacketsPerDatagram ||
            (&datagram != &next.back() && packets != m_series->maximumPacketsPerDatagram) ||
            packets > (std::numeric_limits<std::size_t>::max)() - packetCount)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS prefix descriptors violate their packet geometry"));
        packetCount += packets;
    }
    auto prepared = m_series->cursor->prepareSeries(MediaTsPacketReservationWork{
        m_generation, window.release, window.accessUnitDeadline, window.completionLimit,
        m_materializedDatagrams, count, packetCount, m_series->maximumPacketsPerDatagram});
    if (!prepared) return ::media::Status::failure(prepared.error());
    auto token = prepared.value().takeCommitToken();
    const auto identity = token.reservationIdentity();
    if (!identity) {
        m_series->failed = true;
        m_series->cursor->poison();
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "MPEG-TS next prefix has no publication identity"));
    }
    auto transaction = MediaProtocolDatagramCommitTransaction::bindPrepared(
        MediaTsProtocolCommitReservation(*m_series->cursor, std::move(token), count),
        std::move(allocation));
    if (!transaction) {
        m_series->failed = true;
        return ::media::Status::failure(transaction.error());
    }
    m_series->transaction.emplace(std::move(transaction).value());
    m_series->protocolCommitCompletedSteadyNs.reset();
    // This range has never been published. Existing views retain immutable
    // descriptors in earlier, disjoint ranges of the same backing.
    for (auto& datagram : next) {
        datagram.m_canonicalRelease = window.release;
        datagram.m_completionLimit = window.completionLimit;
    }
    m_series->reservationBegin = m_materializedDatagrams;
    m_series->reservationEnd = m_materializedDatagrams + count;
    m_series->reservationIdentity = identity;
    m_publicationObservation = MediaTsPublicationObservation{window.acceptedWorkObservation,
        identity->cursorIdentity, identity->cursorRevision, identity->publicationIdentity,
        m_series->reservationBegin, m_series->reservationEnd, std::nullopt};
    return ::media::Status::success();
}

::media::Status MediaMpegTsProtocolDatagramBatchBuffer::registerPreparedWire(
    std::span<const MediaWireDatagramBatchInput> inputs,
    std::span<const MediaWireGlobalSequenceReservationEntry> entries,
    std::size_t leadingControlDatagrams)
{
    const auto failure = [] { return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "MPEG-TS prepared wire inputs differ from their exact reserved geometry")); };
    if (!m_series || inputs.size() != entries.size() || leadingControlDatagrams > 1 ||
        inputs.size() <= leadingControlDatagrams) return failure();
    std::lock_guard lock(m_series->mutex);
    const auto count = inputs.size() - leadingControlDatagrams;
    if (m_series->failed || m_preparedWire || !m_series->reservationIdentity ||
        m_materializedDatagrams < m_series->reservationBegin ||
        m_materializedDatagrams > m_series->reservationEnd ||
        count > m_series->reservationEnd - m_materializedDatagrams ||
        count > m_endDatagram - m_materializedDatagrams) return failure();
    if (!m_publicationObservation ||
        m_publicationObservation->acceptedWork.generation != m_generation ||
        m_publicationObservation->cursorIdentity != m_series->reservationIdentity->cursorIdentity ||
        m_publicationObservation->cursorRevision != m_series->reservationIdentity->cursorRevision ||
        m_publicationObservation->publicationIdentity != m_series->reservationIdentity->publicationIdentity ||
        m_publicationObservation->protocolBegin != m_series->reservationBegin ||
        m_publicationObservation->protocolEnd != m_series->reservationEnd) return failure();
    const auto& first = inputs[leadingControlDatagrams];
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const auto& input = inputs[index];
        const auto& entry = entries[index];
        if (input.endpointId != entry.endpointId || input.bytes.size() != entry.payloadBytes ||
            !entry.payloadBytes || entry.materializedAt != entries.front().materializedAt)
            return failure();
        if (!input.protocolDeadlineObservation ||
            input.protocolDeadlineObservation->materializedAt != entry.materializedAt ||
            input.protocolDeadlineObservation->publication != m_publicationObservation ||
            input.protocolDeadlineObservation->protectsMediaPublication != (index < leadingControlDatagrams))
            return failure();
        if (index < leadingControlDatagrams) continue;
        const auto& source = m_series->datagrams.view()[m_materializedDatagrams + index - leadingControlDatagrams];
        if (input.endpointId != first.endpointId || input.canonicalRelease != first.canonicalRelease ||
            input.canonicalRelease != source.canonicalRelease() ||
            input.canonicalDeadline != first.canonicalDeadline ||
            !input.protocolDeadlineObservation ||
            input.protocolDeadlineObservation->originalDeadline != source.canonicalDeadline() ||
            input.protocolDeadlineObservation->prefixCompletionLimit != source.completionLimit() ||
            (index + 1 != inputs.size() && input.bytes.size() != first.bytes.size()) ||
            input.bytes.size() > first.bytes.size()) return failure();
    }
    MediaTsPreparedWireWork work{m_materializedDatagrams,
        {first.endpointId, static_cast<std::uint64_t>(first.bytes.size()),
            static_cast<std::uint64_t>(inputs.back().bytes.size()), count,
            first.canonicalRelease, first.canonicalDeadline}, std::nullopt};
    if (leadingControlDatagrams) work.leadingReport = MediaTsPreparedControlGeometry{
        inputs.front().endpointId, entries.front().payloadBytes,
        inputs.front().canonicalRelease, inputs.front().canonicalDeadline};
    auto status = m_series->cursor->registerPreparedWire(*m_series->reservationIdentity, work);
    if (!status) return status;
    m_preparedWire = PreparedRegistration{*m_series->reservationIdentity, m_materializedDatagrams,
        m_series->reservationBegin, m_series->reservationEnd, count, leadingControlDatagrams};
    return ::media::Status::success();
}

void MediaMpegTsProtocolDatagramBatchBuffer::abandonPreparedWire(std::size_t protocolOffset) noexcept
{
    if (!m_series) return;
    std::unique_lock lock(m_series->mutex);
    if (!m_preparedWire || protocolOffset != m_preparedWire->protocolBegin - m_beginDatagram) return;
    const auto registration = *m_preparedWire;
    m_preparedWire.reset();
    m_series->abandonReservation(registration.identity, registration.reservationBegin, registration.reservationEnd);
    auto waiter = m_series->takeCommitWaiter();
    lock.unlock();
    if (waiter) waiter->notify();
}

::media::Result<MediaProtocolDatagramCommitTransaction>
MediaMpegTsProtocolDatagramBatchBuffer::takeCommitTransaction(
    std::size_t datagrams, MediaOwnedMemoryAllocation allocation,
    const std::shared_ptr<MediaWireGlobalSequenceState>& wireOwner,
    const MediaWireGlobalSequenceReservation& wireReservation,
    std::size_t leadingControlDatagrams)
{
    using Result = ::media::Result<MediaProtocolDatagramCommitTransaction>;
    if (!m_series || !wireOwner || !wireReservation.belongsTo(*wireOwner) ||
        wireReservation.generation() != m_generation || !wireReservation.identity() ||
        leadingControlDatagrams > wireReservation.size() ||
        datagrams != wireReservation.size() - leadingControlDatagrams || datagrams == 0 ||
        datagrams > m_endDatagram - m_materializedDatagrams) {
        return Result::failure(
            ::media::ErrorInfo::internalError(
                "MPEG-TS protocol transaction prefix is absent or out of bounds"));
    }
    std::size_t reservationBegin;
    std::size_t reservationEnd;
    // Serialize range replacement with protocol callbacks. Global commit
    // releases its own lock before entering this series, including on UDP.
    std::unique_lock lock(m_series->mutex);
    {
        reservationBegin = m_series->reservationBegin;
        reservationEnd = m_series->reservationEnd;
        if (m_series->failed || !m_series->reservationIdentity || !m_preparedWire ||
            m_preparedWire->identity != *m_series->reservationIdentity ||
            m_preparedWire->protocolBegin != m_materializedDatagrams || m_preparedWire->count != datagrams ||
            m_preparedWire->leadingControlDatagrams != leadingControlDatagrams ||
            m_materializedDatagrams < reservationBegin ||
            m_materializedDatagrams > reservationEnd || datagrams > reservationEnd - m_materializedDatagrams)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS commit prefix is outside its reserved interval"));
    }
    if (m_series->wireOwner && m_series->wireOwner != wireOwner)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS series cannot change its bound wire owner"));
    const auto snapshot = wireOwner->snapshot();
    if (snapshot.poisoned || snapshot.generation != m_generation)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS wire mapping requires its live generation"));
    if (m_series->wireRange &&
        (!snapshot.lastCommittedSequence ||
         *snapshot.lastCommittedSequence < m_series->wireRange->lastSequence ||
         m_materializedDatagrams != m_series->wireRange->protocolEnd))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS wire mapping cannot replace an unconfirmed or discontinuous range"));
    auto first = wireReservation.sequence(0);
    auto firstMedia = wireReservation.sequence(leadingControlDatagrams);
    auto last = wireReservation.sequence(wireReservation.size() - 1);
    if (!first || !firstMedia || !last)
        return Result::failure(!first ? first.error() : !firstMedia ? firstMedia.error() : last.error());
    // A failed prepared binding destroys the prefix and poisons its series;
    // that destructor acquires the series lock itself.
    const auto reservationIdentity = *m_series->reservationIdentity;
    lock.unlock();
    auto transaction = MediaProtocolDatagramCommitTransaction::bindPrepared(
        MediaTsProtocolCommitPrefix(m_series, m_materializedDatagrams, datagrams,
            reservationBegin, reservationEnd, reservationIdentity),
        std::move(allocation));
    if (transaction) {
        lock.lock();
        if (m_series->failed ||
            !m_series->matchesReservation(reservationIdentity, reservationBegin, reservationEnd)) {
            lock.unlock();
            return Result::failure(::media::ErrorInfo::internalError(
                "MPEG-TS wire mapping lost its series during binding"));
        }
        auto bound = m_series->cursor->bindPreparedWire(reservationIdentity,
            MediaTsBoundWireRange{m_generation, wireReservation.identity(), first.value(), firstMedia.value(),
                last.value(), m_materializedDatagrams, m_materializedDatagrams + datagrams});
        if (!bound) {
            lock.unlock();
            return Result::failure(bound.error());
        }
        m_preparedWire.reset();
        m_series->wireOwner = wireOwner;
        m_series->wireRange = MediaMpegTsProtocolSeriesState::WireRange{
            reservationIdentity, wireReservation.identity(), first.value(), firstMedia.value(), last.value(),
            m_materializedDatagrams, m_materializedDatagrams + datagrams};
        m_materializedDatagrams += datagrams;
        lock.unlock();
    }
    return transaction;
}

std::span<const MediaMpegTsProtocolDatagram>
MediaMpegTsProtocolDatagramBatchBuffer::datagrams() const noexcept
{
    if (!m_series) return {};
    std::lock_guard lock(m_series->mutex);
    const auto end = (std::min)(m_endDatagram, m_series->reservationEnd);
    return m_materializedDatagrams < end ? m_series->datagrams.view().subspan(
        m_materializedDatagrams, end - m_materializedDatagrams)
        : std::span<const MediaMpegTsProtocolDatagram>{};
}

::media::Result<std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>
MediaMpegTsProtocolDatagramBatchBuffer::takePrefixView(
    std::size_t count, MediaOwnedMemoryAllocation allocation)
{
    using Result = ::media::Result<std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>;
    if (!m_series || count == 0 || count > m_endDatagram - m_materializedDatagrams)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS prefix view exceeds its remaining series range"));
    {
        std::lock_guard lock(m_series->mutex);
        if (m_series->failed || m_materializedDatagrams < m_series->reservationBegin ||
            m_materializedDatagrams > m_series->reservationEnd ||
            count > m_series->reservationEnd - m_materializedDatagrams)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS prefix view is outside its reserved interval"));
    }
    try {
        auto view = std::allocate_shared<MediaMpegTsProtocolDatagramBatchBuffer>(
            MediaSharedAllocationAllocator<MediaMpegTsProtocolDatagramBatchBuffer>{
                MediaSharedAllocationAdmitted{m_series->resource,
                    m_series->batchOwnerGeometry, &allocation}}, ConstructionKey{}, m_generation);
        view->m_series = m_series;
        view->m_publicationObservation = m_publicationObservation;
        view->m_beginDatagram = m_materializedDatagrams;
        view->m_materializedDatagrams = m_materializedDatagrams;
        view->m_endDatagram = m_materializedDatagrams + count;
        for (const auto& datagram : m_series->datagrams.view().subspan(m_materializedDatagrams, count))
            view->m_payloadBytes += static_cast<std::uint64_t>(datagram.bytes().size());
        // Publish the source cursor advance only after construction succeeds.
        m_materializedDatagrams += count;
        return Result::success(std::move(view));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS admitted prefix view construction"));
    }
}

MediaSharedAllocationGeometry
MediaMpegTsProtocolDatagramBatchBuffer::prefixAllocationGeometry() noexcept
{
    return MediaProtocolDatagramCommitTransaction::allocationGeometry<MediaTsProtocolCommitPrefix>();
}

std::optional<std::uint64_t>
MediaMpegTsProtocolDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return m_payloadBytes;
}

} // namespace media::ffmpeg::graph
