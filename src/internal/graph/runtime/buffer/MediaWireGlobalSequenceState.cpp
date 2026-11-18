#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/diagnostics/MediaOutputPreparationObservation.h"

#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> advanceQueueResidence(
    std::uint64_t residenceSumNanoseconds,
    std::optional<MediaRunningTime> updatedAt,
    std::size_t queuedDatagrams,
    MediaRunningTime now)
{
    if (now < MediaRunningTime::fromNanoseconds(0) ||
        (updatedAt && now < *updatedAt)) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "wire pacing queue time is negative or non-monotonic"));
    }
    if (!updatedAt || queuedDatagrams == 0) {
        return ::media::Result<std::uint64_t>::success(
            residenceSumNanoseconds);
    }
    const auto delta = static_cast<std::uint64_t>(
        now.nanoseconds() - updatedAt->nanoseconds());
    auto increment = MediaCheckedArithmetic::multiply(
        delta, static_cast<std::uint64_t>(queuedDatagrams),
        "wire pacing queue residence accumulation");
    return increment
        ? MediaCheckedArithmetic::add(
              residenceSumNanoseconds, increment.value(),
              "wire pacing queue residence accumulation")
        : increment;
}

} // namespace

MediaWireGlobalSequenceReservation::MediaWireGlobalSequenceReservation(
    std::shared_ptr<MediaWireGlobalSequenceState> state,
    std::uint64_t reservationIdentity,
    std::uint64_t firstSequence,
    MediaFixedCapacityStorage<std::uint64_t> wireBytes,
    MediaFixedCapacityStorage<MediaRunningTime> materializedAt) noexcept
    : m_state(std::move(state)),
      m_reservationIdentity(reservationIdentity),
      m_firstSequence(firstSequence),
      m_wireBytes(std::move(wireBytes)),
      m_materializedAt(std::move(materializedAt))
{
}

MediaWireGlobalSequenceReservation::MediaWireGlobalSequenceReservation(
    MediaWireGlobalSequenceReservation&& other) noexcept
    : m_state(std::move(other.m_state)),
      m_reservationIdentity(other.m_reservationIdentity),
      m_firstSequence(other.m_firstSequence),
      m_wireBytes(std::move(other.m_wireBytes)),
      m_materializedAt(std::move(other.m_materializedAt)),
      m_committed(other.m_committed)
{
}

MediaWireGlobalSequenceReservation&
MediaWireGlobalSequenceReservation::operator=(
    MediaWireGlobalSequenceReservation&& other) noexcept
{
    if (this == &other) return *this;
    abandon();
    m_state = std::move(other.m_state);
    m_reservationIdentity = other.m_reservationIdentity;
    m_firstSequence = other.m_firstSequence;
    m_wireBytes = std::move(other.m_wireBytes);
    m_materializedAt = std::move(other.m_materializedAt);
    m_committed = other.m_committed;
    return *this;
}

MediaWireGlobalSequenceReservation::~MediaWireGlobalSequenceReservation()
    noexcept
{
    abandon();
}

std::uint64_t
MediaWireGlobalSequenceReservation::generation() const noexcept
{
    return m_state ? m_state->m_generation : 0;
}

std::size_t MediaWireGlobalSequenceReservation::size() const noexcept
{
    return m_wireBytes.size();
}

::media::Result<std::uint64_t>
MediaWireGlobalSequenceReservation::sequence(std::size_t index) const noexcept
{
    if (!m_state || index >= m_wireBytes.size()) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "wire global sequence reservation index is invalid"));
    }
    return ::media::Result<std::uint64_t>::success(
        m_firstSequence + static_cast<std::uint64_t>(index));
}

::media::Status MediaWireGlobalSequenceReservation::markScheduled(
    std::size_t begin,
    std::size_t count,
    MediaRunningTime now) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    return m_state->markStageRange(
        *this, begin, count, now, m_state->m_lastScheduledSequence);
}

::media::Status MediaWireGlobalSequenceReservation::markSubmitted(
    std::size_t begin,
    std::size_t count,
    MediaRunningTime now) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    return m_state->markStageRange(
        *this, begin, count, now, m_state->m_lastSubmittedSequence);
}

::media::Status MediaWireGlobalSequenceReservation::canCommit(
    std::size_t begin, std::size_t count) const noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    std::lock_guard lock(m_state->m_mutex);
    return m_state->canCommitRangeLocked(*this, begin, count);
}

::media::Status MediaWireGlobalSequenceReservation::commit(
    std::size_t begin,
    std::size_t count,
    const MediaDatagramSubmittedService& submitted) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    const auto now = submitted.submittedAt;
    bool completed = false;
    bool notifyWaiters = false;
    {
        std::lock_guard lock(m_state->m_mutex);
        auto ready = m_state->canCommitRangeLocked(*this, begin, count);
        if (!ready) {
            m_state->m_poisoned = true;
            return ready;
        }
        const auto lastSequence = m_firstSequence +
            static_cast<std::uint64_t>(begin + count - 1);
        if (count != 1 || submitted.generation != m_state->m_generation ||
            submitted.globalSequence != lastSequence) {
            m_state->m_poisoned = true;
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "wire submitted service identity differs from its single datagram commit"));
        }
        if (!m_state->m_lastSubmittedSequence ||
            *m_state->m_lastSubmittedSequence != lastSequence) {
            auto marked = m_state->markStageRangeLocked(
                *this, begin, count, now,
                m_state->m_lastSubmittedSequence);
            if (!marked) {
                m_state->m_poisoned = true;
                return marked;
            }
        }
        std::uint64_t committedWireBytes = 0;
        for (std::size_t index = begin; index < begin + count; ++index) {
            if (m_wireBytes[index] >
                (std::numeric_limits<std::uint64_t>::max)() -
                    committedWireBytes) {
                m_state->m_poisoned = true;
                return ::media::Status::failure(
                    ::media::ErrorInfo::internalError(
                        "wire global sequence range byte accounting overflowed"));
            }
            committedWireBytes += m_wireBytes[index];
        }
        if (count > m_state->m_outstandingDatagrams ||
            committedWireBytes > m_state->m_outstandingWireBytes) {
            m_state->m_poisoned = true;
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "wire global sequence range accounting underflowed"));
        }
        const auto queueAccountingAt =
            m_state->m_queueResidenceUpdatedAt &&
                now < *m_state->m_queueResidenceUpdatedAt
            ? *m_state->m_queueResidenceUpdatedAt
            : now;
        auto queueResidence = advanceQueueResidence(
            m_state->m_queueResidenceSumNanoseconds,
            m_state->m_queueResidenceUpdatedAt,
            m_state->m_outstandingDatagrams, queueAccountingAt);
        std::uint64_t committedResidence = 0;
        for (std::size_t index = begin;
             queueResidence && index < begin + count; ++index) {
            if (queueAccountingAt < m_materializedAt[index]) {
                queueResidence = ::media::Result<std::uint64_t>::failure(
                    ::media::ErrorInfo::internalError(
                        "wire pacing queue commit precedes materialization"));
                break;
            }
            auto residence = static_cast<std::uint64_t>(
                queueAccountingAt.nanoseconds() -
                m_materializedAt[index].nanoseconds());
            auto accumulated = MediaCheckedArithmetic::add(
                committedResidence, residence,
                "wire pacing queue committed residence");
            if (!accumulated) {
                queueResidence = ::media::Result<std::uint64_t>::failure(
                    accumulated.error());
                break;
            }
            committedResidence = accumulated.value();
        }
        if (!queueResidence ||
            committedResidence > queueResidence.value()) {
            m_state->m_poisoned = true;
            return ::media::Status::failure(
                !queueResidence
                    ? queueResidence.error()
                    : ::media::ErrorInfo::internalError(
                          "wire pacing queue residence accounting underflowed"));
        }
        m_state->m_queueResidenceSumNanoseconds =
            queueResidence.value() - committedResidence;
        m_state->m_queueResidenceUpdatedAt = queueAccountingAt;
        m_state->m_nextGlobalSequence += static_cast<std::uint64_t>(count);
        m_state->m_reservations.front().committed += count;
        m_state->m_outstandingDatagrams -= count;
        m_state->m_outstandingWireBytes -= committedWireBytes;
        if (m_state->m_outstandingDatagrams == 0) {
            m_state->m_queueResidenceSumNanoseconds = 0;
        }
        if (!m_state->m_commitBeforeNextReservation ||
            m_committed + count == m_wireBytes.size()) {
            notifyWaiters = std::exchange(
                m_state->m_reservationBlocked, false);
        }
        for (std::size_t index = begin; index < begin + count; ++index) {
            m_state->observeResidence(m_materializedAt[index], now);
        }
        m_state->m_lastCommittedSequence =
            lastSequence;
        // Preserve confirmed submission even if frontier arithmetic later
        // fails. Publish under the ledger lock before either waiter wakes.
        m_state->m_lastSubmittedService = submitted;
        m_committed += count;
        if (m_committed == m_wireBytes.size()) {
            m_state->m_lastCompletedReservation = MediaWireCompletedReservationObservation{
                m_reservationIdentity, m_firstSequence, lastSequence, now};
            m_state->m_reservations.pop_front();
            completed = true;
        }
    }
    if (notifyWaiters) m_state->notifyReservationWaiters();
    if (completed) releaseCompleted();
    return ::media::Status::success();
}

void MediaWireGlobalSequenceReservation::releaseCompleted() noexcept
{
    m_state.reset();
}

void MediaWireGlobalSequenceReservation::abandon() noexcept
{
    if (!m_state) return;
    {
        std::lock_guard lock(m_state->m_mutex);
        if (!m_state->m_authorizedPurge && m_committed != m_wireBytes.size()) {
            m_state->m_poisoned = true;
            for (std::size_t index = m_committed;
                 index < m_wireBytes.size(); ++index) {
                --m_state->m_outstandingDatagrams;
                m_state->m_outstandingWireBytes -= m_wireBytes[index];
            }
        }
    }
    m_state.reset();
}

MediaWireGlobalSequenceState::MediaWireGlobalSequenceState(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    std::uint64_t firstGlobalSequence,
    std::size_t maximumOutstandingDatagrams,
    std::uint64_t maximumOutstandingWireBytes,
    bool commitBeforeNextReservation,
    std::shared_ptr<MediaNetworkMemoryResource> storageResource,
    std::unordered_map<std::uint64_t, std::uint64_t>
        endpointWireHeaderBytes,
    std::shared_ptr<MediaRtpProtocolStorageRegistry> protocolStorage,
    MediaDatagramCommitStoragePlan commitStorage,
    std::optional<MediaTsMuxQueueStoragePlan> muxQueueStorage,
    std::optional<MediaTsSeriesStoragePlan> seriesStorage)
    : m_storageResource(std::move(storageResource)), m_commitStorage(commitStorage),
      m_muxQueueStorage(std::move(muxQueueStorage)),
      m_seriesStorage(std::move(seriesStorage)),
      m_protocolStorage(std::move(protocolStorage)),
      m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation),
      m_maximumOutstandingDatagrams(maximumOutstandingDatagrams),
      m_maximumOutstandingWireBytes(maximumOutstandingWireBytes),
      m_commitBeforeNextReservation(commitBeforeNextReservation),
      m_endpointWireHeaderBytes(std::move(endpointWireHeaderBytes)),
      m_nextGlobalSequence(firstGlobalSequence),
      m_projectedNextGlobalSequence(firstGlobalSequence),
      // Every live record has at least one outstanding datagram.
      m_reservations(maximumOutstandingDatagrams, *m_storageResource)
{
}

::media::Result<std::size_t>
MediaWireGlobalSequenceState::reservationStorageFootprint(
    std::uint64_t maximumOutstandingDatagrams)
{
    if (maximumOutstandingDatagrams == 0 || maximumOutstandingDatagrams >
            (std::numeric_limits<std::size_t>::max)() / sizeof(ReservationRecord)) {
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::invalidArgument(
            "Global sequence reservation storage requires addressable positive capacity"));
    }
    return MediaNetworkMemoryResource::allocationFootprint(
        static_cast<std::size_t>(maximumOutstandingDatagrams) * sizeof(ReservationRecord),
        alignof(ReservationRecord));
}

::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>
MediaWireGlobalSequenceState::create(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    std::uint64_t firstGlobalSequence,
    std::size_t maximumOutstandingDatagrams,
    std::uint64_t maximumOutstandingWireBytes,
    bool commitBeforeNextReservation,
    std::shared_ptr<MediaNetworkMemoryResource> storageResource,
    std::unordered_map<std::uint64_t, std::uint64_t>
        endpointWireHeaderBytes,
    std::span<const MediaRtpProtocolStoragePlan> rtpStorageInstances,
    MediaDatagramCommitStoragePlan commitStorage,
    std::optional<MediaTsMuxQueueStoragePlan> muxQueueStorage,
    std::optional<MediaTsSeriesStoragePlan> seriesStorage)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>;
    auto fixedStorage = reservationStorageFootprint(maximumOutstandingDatagrams);
    if (!fixedStorage) return Result::failure(fixedStorage.error());
    MediaOutputPreparationObservation observation("global_sequence", generation,
        fixedStorage.value(), maximumOutstandingDatagrams);
    if (!storageResource || sessionKey.empty() || serviceScopeId.empty() || generation == 0 ||
        maximumOutstandingWireBytes == 0 || endpointWireHeaderBytes.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence state requires service identity and backlog geometry"));
    }
    for (const auto& [endpointId, headerBytes] : endpointWireHeaderBytes) {
        if (endpointId == 0 || headerBytes == 0) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "wire global sequence state requires explicit endpoint wire headers"));
        }
    }
    for (const auto& instance : rtpStorageInstances) {
        if (instance.recordCapacity != maximumOutstandingDatagrams ||
            !endpointWireHeaderBytes.contains(instance.rtpEndpointId) ||
            !endpointWireHeaderBytes.contains(instance.rtcpEndpointId))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTP storage instance differs from global transport geometry"));
    }
    const auto expectedKind = rtpStorageInstances.empty()
        ? MediaDatagramCommitStorageKind::MpegTsUdp : MediaDatagramCommitStorageKind::Rtp;
    const auto& geometry = commitStorage.geometry;
    const auto& batchGeometry = commitStorage.batchGeometry;
    if (commitStorage.kind != expectedKind || !geometry.count || !geometry.bytes ||
        geometry.bytes % geometry.count != 0 || !geometry.alignment ||
        (geometry.alignment & (geometry.alignment - 1)) || !batchGeometry.count || !batchGeometry.bytes ||
        batchGeometry.bytes % batchGeometry.count != 0 || !batchGeometry.alignment ||
        (batchGeometry.alignment & (batchGeometry.alignment - 1)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Global commit storage requires complete matching protocol geometry"));
    if ((expectedKind == MediaDatagramCommitStorageKind::MpegTsUdp &&
         !commitStorage.protocolPrefixGeometry) ||
        (commitStorage.protocolPrefixGeometry && *commitStorage.protocolPrefixGeometry !=
            MediaMpegTsProtocolDatagramBatchBuffer::prefixAllocationGeometry()))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Global protocol prefix storage differs from its compiled model geometry"));
    if (commitStorage.protocolPrefixGeometry.has_value() != muxQueueStorage.has_value())
        return Result::failure(::media::ErrorInfo::invalidArgument("TS generation requires final mux queue storage"));
    if (muxQueueStorage) {
        auto valid = MediaTsMuxQueueStoragePlanner::validate(*muxQueueStorage);
        if (!valid) return Result::failure(valid.error());
    }
    if (muxQueueStorage.has_value() != seriesStorage.has_value())
        return Result::failure(::media::ErrorInfo::invalidArgument("TS generation requires final series storage"));
    if (seriesStorage) {
        auto valid = MediaTsSeriesStoragePlanner::validate(*seriesStorage);
        if (!valid) return Result::failure(valid.error());
    }
    auto protocolStorage = MediaRtpProtocolStorageRegistry::create(rtpStorageInstances, storageResource);
    if (!protocolStorage) return Result::failure(protocolStorage.error());
    try {
        // Container construction can allocate on supported standard libraries.
        // Keep the object owned throughout shared-control-block allocation.
        auto state = std::unique_ptr<MediaWireGlobalSequenceState>(
            new MediaWireGlobalSequenceState(
                std::move(sessionKey), std::move(serviceScopeId), generation,
                firstGlobalSequence, maximumOutstandingDatagrams,
                maximumOutstandingWireBytes, commitBeforeNextReservation,
                std::move(storageResource), std::move(endpointWireHeaderBytes),
                std::move(protocolStorage).value(), commitStorage, std::move(muxQueueStorage), std::move(seriesStorage)));
        return Result::success(
            std::shared_ptr<MediaWireGlobalSequenceState>(std::move(state)));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaWireGlobalSequenceState allocation or shared ownership"));
    }
}

::media::Result<MediaWireGlobalSequenceReservation>
MediaWireGlobalSequenceState::reserve(
    std::span<const MediaWireGlobalSequenceReservationEntry> entries,
    MediaWireGlobalSequencePreparedStorage& storage)
{
    using Result = ::media::Result<MediaWireGlobalSequenceReservation>;
    std::shared_ptr<MediaWireGlobalSequenceState> owner;
    try {
        owner = shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "wire global sequence state has no shared ownership"));
    }
    if (entries.empty() || entries.size() > m_maximumOutstandingDatagrams) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation count is outside its admitted bound"));
    }
    if (storage.wireBytes.capacity() != entries.size() ||
        storage.materializedAt.capacity() != entries.size() ||
        !storage.wireBytes.empty() || !storage.materializedAt.empty() ||
        storage.wireBytes.resource() != m_storageResource.get() ||
        storage.materializedAt.resource() != m_storageResource.get())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Global sequence requires exact empty prepared transaction backing"));
    std::uint64_t totalWireBytes = 0;
    try {
        for (const auto& entry : entries) {
            const auto header = m_endpointWireHeaderBytes.find(entry.endpointId);
            if (header == m_endpointWireHeaderBytes.end() ||
                entry.payloadBytes == 0 ||
                entry.materializedAt < MediaRunningTime::fromNanoseconds(0) ||
                entry.payloadBytes >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        header->second ||
                entry.payloadBytes + header->second >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        totalWireBytes) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "wire global sequence reservation entry is invalid or overflows"));
            }
            const auto cost = entry.payloadBytes + header->second;
            totalWireBytes += cost;
            if (totalWireBytes > m_maximumOutstandingWireBytes) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "wire global sequence reservation bytes exceed its admitted bound"));
            }
        }
    } catch (const std::length_error&) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation storage size overflows"));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire global sequence reservation credits"));
    }
    std::unique_lock lock(m_mutex);
    if (m_authorizedPurge) {
        return Result::failure(::media::ErrorInfo::cancelled(
            "wire global sequence generation was explicitly purged"));
    }
    if (m_poisoned) {
        return Result::failure(::media::ErrorInfo::internalError(
            "wire global sequence state is poisoned by an uncommitted reservation"));
    }
    if (entries.size() >
            (std::numeric_limits<std::uint64_t>::max)() -
                m_projectedNextGlobalSequence ||
        m_nextReservationIdentity ==
            (std::numeric_limits<std::uint64_t>::max)()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation identity would overflow"));
    }
    if ((m_commitBeforeNextReservation && !m_reservations.empty()) ||
        entries.size() >
            m_maximumOutstandingDatagrams - m_outstandingDatagrams ||
        totalWireBytes >
            m_maximumOutstandingWireBytes - m_outstandingWireBytes) {
        m_reservationBlocked = true;
        // Capture the exact rejection predicates under the accounting lock.
        // The shared message alone cannot distinguish serialization from capacity.
        const bool serial = m_commitBeforeNextReservation && !m_reservations.empty();
        const auto usedCount = m_outstandingDatagrams;
        const auto usedBytes = m_outstandingWireBytes;
        const auto projectedSequence = m_projectedNextGlobalSequence;
        const auto head = m_reservations.empty() ? std::optional<ReservationRecord>{}
            : std::optional<ReservationRecord>{m_reservations.front()};
        lock.unlock();
        // Limits and generation are immutable. Never read mutable ledger state
        // after unlock; formatting must not delay the sender's credit release.
        try {
            std::ostringstream detail;
            detail << "wire global sequence reservation exceeds planner backlog capacity"
                << " serial=" << serial
                << " count=" << (entries.size() > m_maximumOutstandingDatagrams - usedCount)
                << " bytes=" << (totalWireBytes > m_maximumOutstandingWireBytes - usedBytes)
                << " request_count=" << entries.size() << " request_bytes=" << totalWireBytes
                << " used_count=" << usedCount << " used_bytes=" << usedBytes
                << " limit_count=" << m_maximumOutstandingDatagrams << " limit_bytes=" << m_maximumOutstandingWireBytes
                << " generation=" << m_generation << " projected_sequence=" << projectedSequence;
            if (head) {
                detail << " head_id=" << head->identity << " head_first=" << head->firstSequence
                    << " head_count=" << head->count << " head_committed=" << head->committed;
            }
            return Result::failure(::media::ErrorInfo::wouldBlock(detail.str()));
        } catch (...) {
            // Optional diagnostics must not replace the original rejection.
        }
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "wire global sequence reservation exceeds planner backlog capacity"));
    }
    auto projectedResidenceSum = m_queueResidenceSumNanoseconds;
    auto projectedResidenceTime = m_queueResidenceUpdatedAt;
    auto projectedDatagrams = m_outstandingDatagrams;
    for (const auto& entry : entries) {
        const auto materialized = entry.materializedAt;
        if (projectedResidenceTime &&
            materialized < *projectedResidenceTime) {
            const auto existingResidence = static_cast<std::uint64_t>(
                projectedResidenceTime->nanoseconds() -
                materialized.nanoseconds());
            auto accumulated = MediaCheckedArithmetic::add(
                projectedResidenceSum, existingResidence,
                "wire pacing queue out-of-order materialization residence");
            if (!accumulated) return Result::failure(accumulated.error());
            projectedResidenceSum = accumulated.value();
            ++projectedDatagrams;
            continue;
        }
        auto advanced = advanceQueueResidence(
            projectedResidenceSum, projectedResidenceTime,
            projectedDatagrams, materialized);
        if (!advanced) return Result::failure(advanced.error());
        projectedResidenceSum = advanced.value();
        projectedResidenceTime = materialized;
        ++projectedDatagrams;
    }
    if (m_reservations.size() == m_reservations.capacity()) {
        return Result::failure(::media::ErrorInfo::internalError(
            "wire reservation ledger exceeded its admitted datagram bound"));
    }
    // Fill already admitted backing only after wire-credit admission.
    // No allocation or storage-pressure wait may follow this check.
    try {
        for (const auto& entry : entries) {
            storage.wireBytes.push_back(entry.payloadBytes + m_endpointWireHeaderBytes.at(entry.endpointId));
            storage.materializedAt.push_back(entry.materializedAt);
        }
    } catch (const std::length_error&) {
        storage.wireBytes.clear();
        storage.materializedAt.clear();
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation storage size overflows"));
    } catch (const std::bad_alloc&) {
        storage.wireBytes.clear();
        storage.materializedAt.clear();
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire global sequence reservation credits"));
    }
    const auto identity = m_nextReservationIdentity++;
    const auto firstSequence = m_projectedNextGlobalSequence;
    m_reservations.push_back(ReservationRecord{
        identity, firstSequence, entries.size(), 0});
    m_projectedNextGlobalSequence +=
        static_cast<std::uint64_t>(entries.size());
    m_outstandingDatagrams = projectedDatagrams;
    m_outstandingWireBytes += totalWireBytes;
    m_queueResidenceSumNanoseconds = projectedResidenceSum;
    m_queueResidenceUpdatedAt = projectedResidenceTime;
    m_highWaterDatagrams = (std::max)(
        m_highWaterDatagrams,
        static_cast<std::uint64_t>(m_outstandingDatagrams));
    m_highWaterWireBytes = (std::max)(
        m_highWaterWireBytes, m_outstandingWireBytes);
    m_lastMaterializedSequence = m_projectedNextGlobalSequence - 1;
    return Result::success(MediaWireGlobalSequenceReservation(
        std::move(owner), identity, firstSequence,
        std::move(storage.wireBytes), std::move(storage.materializedAt)));
}

::media::Result<MediaWirePacingQueueSnapshot>
MediaWireGlobalSequenceState::pacingQueueSnapshot(
    const MediaMasterClock& clock) const
{
    using Result = ::media::Result<MediaWirePacingQueueSnapshot>;
    std::lock_guard lock(m_mutex);
    if (m_poisoned || m_outstandingDatagrams == 0 ||
        m_outstandingWireBytes == 0) {
        return Result::failure(::media::ErrorInfo::internalError(
            "wire pacing queue snapshot requires active unpoisoned work"));
    }
    auto now = clock.now();
    if (!now) return Result::failure(now.error());
    auto residenceSum = advanceQueueResidence(
        m_queueResidenceSumNanoseconds, m_queueResidenceUpdatedAt,
        m_outstandingDatagrams, now.value());
    if (!residenceSum) return Result::failure(residenceSum.error());
    const auto average = residenceSum.value() /
        static_cast<std::uint64_t>(m_outstandingDatagrams);
    if (average > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return Result::failure(::media::ErrorInfo::internalError(
            "wire pacing queue average residence exceeds running-time range"));
    }
    return Result::success(MediaWirePacingQueueSnapshot{
        m_outstandingWireBytes,
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(average)),
        now.value()});
}

::media::Result<MediaRtpProtocolStoragePermit> MediaWireGlobalSequenceState::claimRtpProtocolStorage(
    std::uint64_t rtpEndpointId, std::uint64_t rtcpEndpointId,
    std::size_t capacity, std::shared_ptr<MediaNodeWakeup> wakeup)
{
    std::lock_guard lock(m_mutex);
    if (m_authorizedPurge || m_poisoned)
        return ::media::Result<MediaRtpProtocolStoragePermit>::failure(
            ::media::ErrorInfo::invalidArgument("Cannot create RTP state in an inactive generation"));
    return m_protocolStorage->claim(rtpEndpointId, rtcpEndpointId, capacity, std::move(wakeup));
}

::media::Status MediaWireGlobalSequenceState::registerReservationWakeup(
    std::uint64_t endpointId,
    std::shared_ptr<MediaNodeWakeup> wakeup)
{
    if (!wakeup) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation wakeup is null"));
    }
    std::lock_guard lock(m_mutex);
    if (m_endpointWireHeaderBytes.find(endpointId) ==
        m_endpointWireHeaderBytes.end()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation wakeup endpoint is outside the service scope"));
    }
    const auto existing = m_reservationWakeups.find(endpointId);
    if (existing != m_reservationWakeups.end()) {
        auto current = existing->second.lock();
        if (current && current != wakeup) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "wire global sequence endpoint already has another reservation wakeup"));
        }
        existing->second = std::move(wakeup);
        return ::media::Status::success();
    }
    try {
        m_reservationWakeups.emplace(endpointId, std::move(wakeup));
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "wire global sequence reservation wakeup registry"));
    }
    return ::media::Status::success();
}

void MediaWireGlobalSequenceState::notifyReservationWaiters() noexcept
{
    std::lock_guard lock(m_mutex);
    m_protocolStorage->notify();
    for (auto& [endpointId, weakWakeup] : m_reservationWakeups) {
        (void)endpointId;
        if (auto wakeup = weakWakeup.lock()) wakeup->notify();
    }
}

::media::Status MediaWireGlobalSequenceState::markStageRange(
    const MediaWireGlobalSequenceReservation& reservation,
    std::size_t begin,
    std::size_t count,
    MediaRunningTime now,
    std::optional<std::uint64_t>& lastStageSequence) noexcept
{
    std::lock_guard lock(m_mutex);
    return markStageRangeLocked(
        reservation, begin, count, now, lastStageSequence);
}

::media::Status MediaWireGlobalSequenceState::markStageRangeLocked(
    const MediaWireGlobalSequenceReservation& reservation,
    std::size_t begin,
    std::size_t count,
    MediaRunningTime now,
    std::optional<std::uint64_t>& lastStageSequence) noexcept
{
    if (m_authorizedPurge || m_poisoned || count == 0 || begin < reservation.m_committed ||
        begin > reservation.m_wireBytes.size() ||
        count > reservation.m_wireBytes.size() - begin) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence lifecycle range is stale or invalid"));
    }
    const auto firstSequence = reservation.m_firstSequence +
        static_cast<std::uint64_t>(begin);
    const auto expectedSequence = lastStageSequence
        ? *lastStageSequence + 1
        : m_nextGlobalSequence;
    if (firstSequence != expectedSequence) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence lifecycle range is not strictly continuous"));
    }
    for (std::size_t index = begin; index < begin + count; ++index) {
        if (now < reservation.m_materializedAt[index]) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "wire global sequence lifecycle range precedes materialization"));
        }
    }
    for (std::size_t index = begin; index < begin + count; ++index) {
        observeResidence(reservation.m_materializedAt[index], now);
    }
    lastStageSequence = firstSequence + static_cast<std::uint64_t>(count - 1);
    return ::media::Status::success();
}

::media::Status MediaWireGlobalSequenceState::canCommitRangeLocked(
    const MediaWireGlobalSequenceReservation& reservation,
    std::size_t begin,
    std::size_t count) const noexcept
{
    if (m_authorizedPurge || m_poisoned || count == 0 || m_reservations.empty() ||
        m_reservations.front().identity != reservation.m_reservationIdentity ||
        m_reservations.front().firstSequence != reservation.m_firstSequence ||
        m_reservations.front().count != reservation.m_wireBytes.size() ||
        begin != reservation.m_committed ||
        begin > reservation.m_wireBytes.size() ||
        count > reservation.m_wireBytes.size() - begin ||
        m_reservations.front().committed != begin ||
        m_nextGlobalSequence != reservation.m_firstSequence +
            static_cast<std::uint64_t>(begin)) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence commit range is stale, reordered, or inactive"));
    }
    return ::media::Status::success();
}

void MediaWireGlobalSequenceState::observeResidence(
    MediaRunningTime materializedAt, MediaRunningTime now) noexcept
{
    const auto residence = now.nanoseconds() - materializedAt.nanoseconds();
    if (residence >= 0) {
        m_maximumResidenceNanoseconds = (std::max)(
            m_maximumResidenceNanoseconds,
            static_cast<std::uint64_t>(residence));
    }
}

::media::Status MediaWireGlobalSequenceState::authorizeGenerationPurge(
    const MediaAvGenerationPurge& purge)
{
    {
        std::lock_guard lock(m_mutex);
        if (purge.oldGeneration != m_generation ||
            purge.nextGeneration <= purge.oldGeneration || purge.transitionSequence == 0 ||
            m_poisoned || (m_authorizedPurge &&
                (m_authorizedPurge->oldGeneration != purge.oldGeneration ||
                 m_authorizedPurge->nextGeneration != purge.nextGeneration ||
                 m_authorizedPurge->transitionSequence != purge.transitionSequence))) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "wire global sequence purge differs from its generation transaction"));
        }
        if (m_authorizedPurge) return ::media::Status::success();
        m_authorizedPurge = purge;
        m_lastSubmittedService.reset();
        m_cancelledDatagrams = m_outstandingDatagrams;
        m_cancelledWireBytes = m_outstandingWireBytes;
        m_outstandingDatagrams = 0;
        m_outstandingWireBytes = 0;
        m_queueResidenceSumNanoseconds = 0;
        m_reservations.clear();
        m_reservationBlocked = false;
    }
    notifyReservationWaiters();
    return ::media::Status::success();
}

MediaWireGlobalSequenceSnapshot
MediaWireGlobalSequenceState::snapshot() const noexcept
{
    std::lock_guard lock(m_mutex);
    return MediaWireGlobalSequenceSnapshot{
        m_generation,
        m_nextGlobalSequence,
        !m_reservations.empty(),
        m_poisoned,
        m_cancelledDatagrams,
        m_cancelledWireBytes,
        static_cast<std::uint64_t>(m_outstandingDatagrams),
        m_outstandingWireBytes,
        m_highWaterDatagrams,
        m_highWaterWireBytes,
        m_maximumResidenceNanoseconds,
        m_lastMaterializedSequence,
        m_lastScheduledSequence,
        m_lastSubmittedSequence,
        m_lastCommittedSequence,
        m_lastCompletedReservation,
        m_lastSubmittedService};
}

} // namespace media::ffmpeg::graph
