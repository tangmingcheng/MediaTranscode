#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaWireGlobalSequenceReservation::MediaWireGlobalSequenceReservation(
    std::shared_ptr<MediaWireGlobalSequenceState> state,
    std::size_t count,
    std::unique_lock<std::mutex> lock) noexcept
    : m_state(std::move(state)),
      m_firstSequence(m_state->m_nextGlobalSequence),
      m_count(count),
      m_lock(std::move(lock))
{
}

MediaWireGlobalSequenceReservation::MediaWireGlobalSequenceReservation(
    MediaWireGlobalSequenceReservation&& other) noexcept
    : m_state(std::move(other.m_state)),
      m_firstSequence(other.m_firstSequence),
      m_count(other.m_count),
      m_committed(other.m_committed),
      m_lock(std::move(other.m_lock))
{
}

MediaWireGlobalSequenceReservation&
MediaWireGlobalSequenceReservation::operator=(
    MediaWireGlobalSequenceReservation&& other) noexcept
{
    if (this == &other) return *this;
    abandon();
    m_state = std::move(other.m_state);
    m_firstSequence = other.m_firstSequence;
    m_count = other.m_count;
    m_committed = other.m_committed;
    m_lock = std::move(other.m_lock);
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
    return m_count;
}

::media::Result<std::uint64_t>
MediaWireGlobalSequenceReservation::sequence(std::size_t index) const noexcept
{
    if (!m_state || index >= m_count) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "wire global sequence reservation index is invalid"));
    }
    return ::media::Result<std::uint64_t>::success(
        m_firstSequence + static_cast<std::uint64_t>(index));
}

::media::Status MediaWireGlobalSequenceReservation::canCommit(
    std::size_t index) const noexcept
{
    if (!m_state || !m_lock.owns_lock() || m_state->m_poisoned ||
        !m_state->m_reservationActive || index != m_committed ||
        index >= m_count ||
        m_state->m_nextGlobalSequence !=
            m_firstSequence + static_cast<std::uint64_t>(index)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "wire global sequence commit is stale, reordered, or inactive"));
    }
    return ::media::Status::success();
}

::media::Status MediaWireGlobalSequenceReservation::commit(
    std::size_t index) noexcept
{
    auto valid = canCommit(index);
    if (!valid) return valid;
    ++m_state->m_nextGlobalSequence;
    ++m_committed;
    if (m_committed == m_count) releaseCompleted();
    return ::media::Status::success();
}

void MediaWireGlobalSequenceReservation::releaseCompleted() noexcept
{
    m_state->m_reservationActive = false;
    if (m_lock.owns_lock()) m_lock.unlock();
    m_state.reset();
}

void MediaWireGlobalSequenceReservation::abandon() noexcept
{
    if (!m_state) return;
    if (m_committed != m_count) m_state->m_poisoned = true;
    m_state->m_reservationActive = false;
    if (m_lock.owns_lock()) m_lock.unlock();
    m_state.reset();
}

MediaWireGlobalSequenceState::MediaWireGlobalSequenceState(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    std::uint64_t firstGlobalSequence) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation),
      m_nextGlobalSequence(firstGlobalSequence)
{
}

::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>
MediaWireGlobalSequenceState::create(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    std::uint64_t firstGlobalSequence)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>;
    if (sessionKey.empty() || serviceScopeId.empty() || generation == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence state requires session, service scope, and generation identity"));
    }
    auto* state = new (std::nothrow) MediaWireGlobalSequenceState(
        std::move(sessionKey), std::move(serviceScopeId), generation,
        firstGlobalSequence);
    if (!state) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaWireGlobalSequenceState"));
    }
    try {
        return Result::success(
            std::shared_ptr<MediaWireGlobalSequenceState>(state));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaWireGlobalSequenceState shared ownership"));
    }
}

::media::Result<MediaWireGlobalSequenceReservation>
MediaWireGlobalSequenceState::reserve(std::size_t count)
{
    using Result = ::media::Result<MediaWireGlobalSequenceReservation>;
    std::shared_ptr<MediaWireGlobalSequenceState> owner;
    try {
        owner = shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "wire global sequence state has no shared ownership"));
    }
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock() || m_reservationActive) {
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "wire global sequence already has an exclusive reservation"));
    }
    if (m_poisoned) {
        return Result::failure(::media::ErrorInfo::internalError(
            "wire global sequence state is poisoned by an uncommitted reservation"));
    }
    if (count == 0 || count >
            (std::numeric_limits<std::uint64_t>::max)() -
                m_nextGlobalSequence) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire global sequence reservation is empty or would overflow"));
    }
    m_reservationActive = true;
    return Result::success(MediaWireGlobalSequenceReservation(
        std::move(owner), count, std::move(lock)));
}

MediaWireGlobalSequenceSnapshot
MediaWireGlobalSequenceState::snapshot() const noexcept
{
    std::lock_guard lock(m_mutex);
    return MediaWireGlobalSequenceSnapshot{
        m_generation,
        m_nextGlobalSequence,
        m_reservationActive,
        m_poisoned};
}

} // namespace media::ffmpeg::graph
