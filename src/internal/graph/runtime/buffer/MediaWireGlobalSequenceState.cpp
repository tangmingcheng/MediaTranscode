#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaWireGlobalSequenceReservation::MediaWireGlobalSequenceReservation(
    std::shared_ptr<MediaWireGlobalSequenceState> state,
    std::uint64_t reservationIdentity,
    std::uint64_t firstSequence,
    std::vector<std::uint64_t> wireBytes,
    std::vector<MediaRunningTime> materializedAt) noexcept
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
    std::size_t index, MediaRunningTime now) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    return m_state->markStage(
        *this, index, now, m_state->m_lastScheduledSequence);
}

::media::Status MediaWireGlobalSequenceReservation::markSubmitted(
    std::size_t index, MediaRunningTime now) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    return m_state->markStage(
        *this, index, now, m_state->m_lastSubmittedSequence);
}

::media::Status MediaWireGlobalSequenceReservation::canCommit(
    std::size_t index) const noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    std::lock_guard lock(m_state->m_mutex);
    if (m_state->m_poisoned || m_state->m_reservations.empty() ||
        m_state->m_reservations.front().identity != m_reservationIdentity ||
        m_state->m_reservations.front().firstSequence != m_firstSequence ||
        m_state->m_reservations.front().count != m_wireBytes.size() ||
        index != m_committed || index >= m_wireBytes.size() ||
        m_state->m_reservations.front().committed != index ||
        m_state->m_nextGlobalSequence !=
            m_firstSequence + static_cast<std::uint64_t>(index)) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence commit is stale, reordered, or inactive"));
    }
    return ::media::Status::success();
}

::media::Status MediaWireGlobalSequenceReservation::commit(
    std::size_t index, MediaRunningTime now) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence reservation is inactive"));
    }
    bool completed = false;
    {
        std::lock_guard lock(m_state->m_mutex);
        if (m_state->m_poisoned || m_state->m_reservations.empty() ||
            m_state->m_reservations.front().identity != m_reservationIdentity ||
            m_state->m_reservations.front().firstSequence != m_firstSequence ||
            m_state->m_reservations.front().count != m_wireBytes.size() ||
            index != m_committed || index >= m_wireBytes.size() ||
            m_state->m_reservations.front().committed != index ||
            m_state->m_nextGlobalSequence !=
                m_firstSequence + static_cast<std::uint64_t>(index)) {
            m_state->m_poisoned = true;
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "wire global sequence commit is stale, reordered, or inactive"));
        }
        ++m_state->m_nextGlobalSequence;
        ++m_state->m_reservations.front().committed;
        --m_state->m_outstandingDatagrams;
        m_state->m_outstandingWireBytes -= m_wireBytes[index];
        m_state->observeResidence(m_materializedAt[index], now);
        m_state->m_lastCommittedSequence =
            m_firstSequence + static_cast<std::uint64_t>(index);
        ++m_committed;
        if (m_committed == m_wireBytes.size()) {
            m_state->m_reservations.pop_front();
            completed = true;
        }
    }
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
        if (m_committed != m_wireBytes.size()) {
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
    std::unordered_map<std::uint64_t, std::uint64_t>
        endpointWireHeaderBytes) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation),
      m_maximumOutstandingDatagrams(maximumOutstandingDatagrams),
      m_maximumOutstandingWireBytes(maximumOutstandingWireBytes),
      m_endpointWireHeaderBytes(std::move(endpointWireHeaderBytes)),
      m_nextGlobalSequence(firstGlobalSequence),
      m_projectedNextGlobalSequence(firstGlobalSequence)
{
}

::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>
MediaWireGlobalSequenceState::create(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    std::uint64_t firstGlobalSequence,
    std::size_t maximumOutstandingDatagrams,
    std::uint64_t maximumOutstandingWireBytes,
    std::unordered_map<std::uint64_t, std::uint64_t>
        endpointWireHeaderBytes)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>;
    if (sessionKey.empty() || serviceScopeId.empty() || generation == 0 ||
        maximumOutstandingDatagrams == 0 ||
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
    auto* state = new (std::nothrow) MediaWireGlobalSequenceState(
        std::move(sessionKey), std::move(serviceScopeId), generation,
        firstGlobalSequence, maximumOutstandingDatagrams,
        maximumOutstandingWireBytes, std::move(endpointWireHeaderBytes));
    if (!state) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaWireGlobalSequenceState"));
    }
    try {
        return Result::success(
            std::shared_ptr<MediaWireGlobalSequenceState>(state));
    } catch (const std::bad_alloc&) {
        delete state;
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaWireGlobalSequenceState shared ownership"));
    }
}

::media::Result<MediaWireGlobalSequenceReservation>
MediaWireGlobalSequenceState::reserve(
    std::span<const MediaWireGlobalSequenceReservationEntry> entries)
{
    using Result = ::media::Result<MediaWireGlobalSequenceReservation>;
    std::shared_ptr<MediaWireGlobalSequenceState> owner;
    try {
        owner = shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "wire global sequence state has no shared ownership"));
    }
    if (entries.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation is empty or would overflow"));
    }
    std::vector<std::uint64_t> wireBytes;
    std::vector<MediaRunningTime> materializedAt;
    std::uint64_t totalWireBytes = 0;
    try {
        wireBytes.reserve(entries.size());
        materializedAt.reserve(entries.size());
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
            wireBytes.push_back(cost);
            materializedAt.push_back(entry.materializedAt);
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire global sequence reservation credits"));
    }
    std::lock_guard lock(m_mutex);
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
    if (entries.size() >
            m_maximumOutstandingDatagrams - m_outstandingDatagrams ||
        totalWireBytes >
            m_maximumOutstandingWireBytes - m_outstandingWireBytes) {
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "wire global sequence reservation exceeds planner backlog capacity"));
    }
    const auto identity = m_nextReservationIdentity++;
    const auto firstSequence = m_projectedNextGlobalSequence;
    try {
        m_reservations.push_back(ReservationRecord{
            identity, firstSequence, entries.size(), 0});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire global sequence reservation ledger"));
    }
    m_projectedNextGlobalSequence +=
        static_cast<std::uint64_t>(entries.size());
    m_outstandingDatagrams += entries.size();
    m_outstandingWireBytes += totalWireBytes;
    m_highWaterDatagrams = (std::max)(
        m_highWaterDatagrams,
        static_cast<std::uint64_t>(m_outstandingDatagrams));
    m_highWaterWireBytes = (std::max)(
        m_highWaterWireBytes, m_outstandingWireBytes);
    m_lastMaterializedSequence = m_projectedNextGlobalSequence - 1;
    return Result::success(MediaWireGlobalSequenceReservation(
        std::move(owner), identity, firstSequence,
        std::move(wireBytes), std::move(materializedAt)));
}

::media::Status MediaWireGlobalSequenceState::markStage(
    const MediaWireGlobalSequenceReservation& reservation,
    std::size_t index,
    MediaRunningTime now,
    std::optional<std::uint64_t>& lastStageSequence) noexcept
{
    std::lock_guard lock(m_mutex);
    if (m_poisoned || index < reservation.m_committed ||
        index >= reservation.m_wireBytes.size() ||
        now < reservation.m_materializedAt[index]) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence lifecycle stage is stale or invalid"));
    }
    const auto sequence = reservation.m_firstSequence +
        static_cast<std::uint64_t>(index);
    if (lastStageSequence && sequence < *lastStageSequence) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire global sequence lifecycle stage regressed"));
    }
    observeResidence(reservation.m_materializedAt[index], now);
    lastStageSequence = sequence;
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

MediaWireGlobalSequenceSnapshot
MediaWireGlobalSequenceState::snapshot() const noexcept
{
    std::lock_guard lock(m_mutex);
    return MediaWireGlobalSequenceSnapshot{
        m_generation,
        m_nextGlobalSequence,
        !m_reservations.empty(),
        m_poisoned,
        static_cast<std::uint64_t>(m_outstandingDatagrams),
        m_outstandingWireBytes,
        m_highWaterDatagrams,
        m_highWaterWireBytes,
        m_maximumResidenceNanoseconds,
        m_lastMaterializedSequence,
        m_lastScheduledSequence,
        m_lastSubmittedSequence,
        m_lastCommittedSequence};
}

} // namespace media::ffmpeg::graph
