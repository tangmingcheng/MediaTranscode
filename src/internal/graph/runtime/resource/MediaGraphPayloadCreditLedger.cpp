#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

class MediaGraphPayloadCreditState final {
public:
    explicit MediaGraphPayloadCreditState(MediaGraphPayloadCreditPlan plan)
        : plan(std::move(plan)) {}

    MediaGraphPayloadCreditPlan plan;
    mutable std::mutex mutex;
    MediaGraphPayloadCreditSnapshot snapshot;
    std::weak_ptr<MediaGraphPayloadCreditReleaseObserver> releaseObserver;
};

MediaGraphPayloadCreditLease::MediaGraphPayloadCreditLease(
    std::shared_ptr<MediaGraphPayloadCreditState> state,
    std::uint64_t bytes) noexcept
    : m_state(std::move(state)), m_bytes(bytes)
{
}

MediaGraphPayloadCreditLease::~MediaGraphPayloadCreditLease()
{
    release();
}

MediaGraphPayloadCreditLease::MediaGraphPayloadCreditLease(
    MediaGraphPayloadCreditLease&& other) noexcept
    : m_state(std::move(other.m_state)), m_bytes(other.m_bytes)
{
    other.m_bytes = 0;
}

MediaGraphPayloadCreditLease& MediaGraphPayloadCreditLease::operator=(
    MediaGraphPayloadCreditLease&& other) noexcept
{
    if (this == &other) return *this;
    release();
    m_state = std::move(other.m_state);
    m_bytes = other.m_bytes;
    other.m_bytes = 0;
    return *this;
}

::media::Status MediaGraphPayloadCreditLease::shrinkTo(
    std::uint64_t bytes) noexcept
{
    if (!m_state || bytes == 0 || bytes > m_bytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "graph payload credit lease only permits a positive shrink"));
    }
    std::shared_ptr<MediaGraphPayloadCreditReleaseObserver> observer;
    {
        std::lock_guard lock(m_state->mutex);
        const std::uint64_t released = m_bytes - bytes;
        if (released > m_state->snapshot.currentBytes) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "graph payload credit accounting underflowed while shrinking"));
        }
        m_state->snapshot.currentBytes -= released;
        m_bytes = bytes;
        if (released > 0) observer = m_state->releaseObserver.lock();
    }
    if (observer) observer->onGraphPayloadCreditReleased();
    return ::media::Status::success();
}

void MediaGraphPayloadCreditLease::release() noexcept
{
    if (!m_state) return;
    std::shared_ptr<MediaGraphPayloadCreditReleaseObserver> observer;
    {
        std::lock_guard lock(m_state->mutex);
        if (m_bytes > m_state->snapshot.currentBytes ||
            m_state->snapshot.currentObjects == 0) {
            std::terminate();
        }
        m_state->snapshot.currentBytes -= m_bytes;
        --m_state->snapshot.currentObjects;
        ++m_state->snapshot.releases;
        observer = m_state->releaseObserver.lock();
    }
    if (observer) observer->onGraphPayloadCreditReleased();
    m_bytes = 0;
    m_state.reset();
}

MediaGraphPayloadCreditLedger::MediaGraphPayloadCreditLedger(
    MediaGraphPayloadCreditPlan plan,
    std::shared_ptr<MediaGraphPayloadCreditState> state) noexcept
    : m_plan(std::move(plan)), m_state(std::move(state))
{
}

::media::Result<std::shared_ptr<MediaGraphPayloadCreditLedger>>
MediaGraphPayloadCreditLedger::create(MediaGraphPayloadCreditPlan plan)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaGraphPayloadCreditLedger>>;
    if (plan.maximumBytes == 0 || plan.maximumObjects == 0 ||
        plan.maximumUnitBytes == 0 ||
        plan.maximumUnitBytes > plan.maximumBytes || plan.authority.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "graph payload credit plan requires bounded bytes, objects, unit, and authority"));
    }
    try {
        auto state = std::make_shared<MediaGraphPayloadCreditState>(plan);
        return Result::success(std::shared_ptr<MediaGraphPayloadCreditLedger>(
            new MediaGraphPayloadCreditLedger(
                std::move(plan), std::move(state))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaGraphPayloadCreditLedger"));
    }
}

::media::Result<MediaGraphPayloadCreditLease>
MediaGraphPayloadCreditLedger::tryReserve(std::uint64_t bytes) noexcept
{
    using Result = ::media::Result<MediaGraphPayloadCreditLease>;
    if (bytes == 0 || bytes > m_plan.maximumUnitBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "graph payload credit reservation exceeds its unit contract"));
    }
    std::lock_guard lock(m_state->mutex);
    const bool bytePressure = bytes > m_plan.maximumBytes ||
        m_state->snapshot.currentBytes > m_plan.maximumBytes - bytes;
    const bool objectPressure =
        m_state->snapshot.currentObjects >= m_plan.maximumObjects;
    if (bytePressure || objectPressure) {
        ++m_state->snapshot.pressureFailures;
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "graph payload credit ledger is at its planner hard bound"));
    }
    m_state->snapshot.currentBytes += bytes;
    ++m_state->snapshot.currentObjects;
    m_state->snapshot.highWaterBytes = (std::max)(
        m_state->snapshot.highWaterBytes,
        m_state->snapshot.currentBytes);
    m_state->snapshot.highWaterObjects = (std::max)(
        m_state->snapshot.highWaterObjects,
        m_state->snapshot.currentObjects);
    ++m_state->snapshot.reservations;
    return Result::success(MediaGraphPayloadCreditLease(m_state, bytes));
}

MediaGraphPayloadCreditSnapshot
MediaGraphPayloadCreditLedger::snapshot() const noexcept
{
    std::lock_guard lock(m_state->mutex);
    return m_state->snapshot;
}

void MediaGraphPayloadCreditLedger::setReleaseObserver(
    std::weak_ptr<MediaGraphPayloadCreditReleaseObserver> observer) noexcept
{
    std::lock_guard lock(m_state->mutex);
    m_state->releaseObserver = std::move(observer);
}

} // namespace media::ffmpeg::graph
