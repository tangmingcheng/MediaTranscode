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
    if (!m_state || bytes > m_bytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "graph payload credit lease only permits shrinking"));
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
    if (!plan.isCompleteAndValid()) {
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
    auto batch = tryReserveBatch(std::span<const std::uint64_t>(&bytes, 1));
    if (!batch) {
        return ::media::Result<MediaGraphPayloadCreditLease>::failure(
            batch.error());
    }
    auto leases = std::move(batch).value();
    return ::media::Result<MediaGraphPayloadCreditLease>::success(
        std::move(leases.front()));
}

::media::Result<std::vector<MediaGraphPayloadCreditLease>>
MediaGraphPayloadCreditLedger::tryReserveBatch(
    std::span<const std::uint64_t> bytes) noexcept
{
    using Result =
        ::media::Result<std::vector<MediaGraphPayloadCreditLease>>;
    if (bytes.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "graph payload credit batch reservation is empty"));
    }
    std::uint64_t totalBytes = 0;
    for (const auto value : bytes) {
        if (value > m_plan.maximumUnitBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "graph payload credit reservation exceeds its unit contract"));
        }
        if (value > (std::numeric_limits<std::uint64_t>::max)() - totalBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "graph payload credit batch byte total is not representable"));
        }
        totalBytes += value;
    }
    std::vector<MediaGraphPayloadCreditLease> leases;
    try {
        leases.reserve(bytes.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "graph payload credit batch lease identities"));
    }
    {
        std::lock_guard lock(m_state->mutex);
        const auto objectCount = static_cast<std::uint64_t>(bytes.size());
        const bool bytePressure = totalBytes > m_plan.maximumBytes ||
            m_state->snapshot.currentBytes >
                m_plan.maximumBytes - totalBytes;
        const bool objectPressure = objectCount > m_plan.maximumObjects ||
            m_state->snapshot.currentObjects >
                m_plan.maximumObjects - objectCount;
        if (bytePressure || objectPressure) {
            ++m_state->snapshot.pressureFailures;
            return Result::failure(::media::ErrorInfo::wouldBlock(
                "graph payload credit ledger is at its planner hard bound"));
        }
        for (const auto value : bytes) {
            leases.push_back(MediaGraphPayloadCreditLease(m_state, value));
        }
        m_state->snapshot.currentBytes += totalBytes;
        m_state->snapshot.currentObjects += objectCount;
        m_state->snapshot.highWaterBytes = (std::max)(
            m_state->snapshot.highWaterBytes,
            m_state->snapshot.currentBytes);
        m_state->snapshot.highWaterObjects = (std::max)(
            m_state->snapshot.highWaterObjects,
            m_state->snapshot.currentObjects);
        m_state->snapshot.reservations += objectCount;
    }
    return Result::success(std::move(leases));
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
