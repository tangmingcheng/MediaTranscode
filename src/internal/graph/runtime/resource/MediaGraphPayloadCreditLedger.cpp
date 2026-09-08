#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <algorithm>
#include <deque>
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
    struct Pending final {
        MediaNodeId producer;
        std::vector<std::uint64_t> bytes;
        std::uint64_t totalBytes = 0;
        std::weak_ptr<MediaNodeWakeup> wakeup;
        bool granted = false;
    };
    std::deque<Pending> waiters;
    bool cancelled = false;
};

namespace {

bool sameDemand(const MediaGraphPayloadCreditState::Pending& pending,
                std::span<const std::uint64_t> bytes) noexcept
{
    return pending.bytes.size() == bytes.size() &&
        std::equal(pending.bytes.begin(), pending.bytes.end(), bytes.begin());
}

bool fits(const MediaGraphPayloadCreditState& state,
          const MediaGraphPayloadCreditState::Pending& pending) noexcept
{
    const auto objects = static_cast<std::uint64_t>(pending.bytes.size());
    return pending.totalBytes <= state.plan.maximumBytes &&
        state.snapshot.currentBytes <= state.plan.maximumBytes - pending.totalBytes &&
        objects <= state.plan.maximumObjects &&
        state.snapshot.currentObjects <= state.plan.maximumObjects - objects;
}

std::shared_ptr<MediaNodeWakeup> promoteOne(
    MediaGraphPayloadCreditState& state) noexcept
{
    for (auto it = state.waiters.begin(); it != state.waiters.end();) {
        auto wakeup = it->wakeup.lock();
        if (!wakeup) {
            it = state.waiters.erase(it);
            continue;
        }
        if (it->granted || !fits(state, *it)) {
            ++it;
            continue;
        }
        const auto objects = static_cast<std::uint64_t>(it->bytes.size());
        state.snapshot.currentBytes += it->totalBytes;
        state.snapshot.currentObjects += objects;
        state.snapshot.highWaterBytes = (std::max)(
            state.snapshot.highWaterBytes, state.snapshot.currentBytes);
        state.snapshot.highWaterObjects = (std::max)(
            state.snapshot.highWaterObjects, state.snapshot.currentObjects);
        it->granted = true;
        return wakeup;
    }
    return {};
}

} // namespace

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
    std::shared_ptr<MediaNodeWakeup> wakeup;
    {
        std::lock_guard lock(m_state->mutex);
        const std::uint64_t released = m_bytes - bytes;
        if (released > m_state->snapshot.currentBytes) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "graph payload credit accounting underflowed while shrinking"));
        }
        m_state->snapshot.currentBytes -= released;
        m_bytes = bytes;
        if (released > 0) wakeup = promoteOne(*m_state);
    }
    if (wakeup) wakeup->notify();
    return ::media::Status::success();
}

void MediaGraphPayloadCreditLease::release() noexcept
{
    if (!m_state) return;
    std::shared_ptr<MediaNodeWakeup> wakeup;
    {
        std::lock_guard lock(m_state->mutex);
        if (m_bytes > m_state->snapshot.currentBytes ||
            m_state->snapshot.currentObjects == 0) {
            std::terminate();
        }
        m_state->snapshot.currentBytes -= m_bytes;
        --m_state->snapshot.currentObjects;
        ++m_state->snapshot.releases;
        wakeup = promoteOne(*m_state);
    }
    if (wakeup) wakeup->notify();
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

::media::Result<std::vector<MediaGraphPayloadCreditLease>>
MediaGraphPayloadCreditLedger::tryReserveOrArm(
    MediaNodeId producer,
    std::span<const std::uint64_t> bytes,
    std::shared_ptr<MediaNodeWakeup> wakeup) noexcept
{
    using Result =
        ::media::Result<std::vector<MediaGraphPayloadCreditLease>>;
    if (!producer.isValid() || !wakeup || bytes.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "graph payload waiter requires producer, demand, and wake handle"));
    }
    std::uint64_t totalBytes = 0;
    for (const auto value : bytes) {
        if (value > m_plan.maximumUnitBytes ||
            value > (std::numeric_limits<std::uint64_t>::max)() - totalBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "graph payload waiter demand exceeds its unit contract"));
        }
        totalBytes += value;
    }
    std::vector<MediaGraphPayloadCreditLease> leases;
    std::vector<std::uint64_t> demand;
    try {
        leases.reserve(bytes.size());
        demand.assign(bytes.begin(), bytes.end());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "graph payload waiter identities"));
    }

    std::shared_ptr<MediaNodeWakeup> nextWakeup;
    {
        std::lock_guard lock(m_state->mutex);
        if (m_state->cancelled) {
            return Result::failure(::media::ErrorInfo::cancelled(
                "graph payload credit waiters were cancelled"));
        }
        auto existing = std::find_if(
            m_state->waiters.begin(), m_state->waiters.end(),
            [producer](const auto& waiter) {
                return waiter.producer == producer;
            });
        if (existing != m_state->waiters.end()) {
            if (!sameDemand(*existing, bytes)) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "graph payload producer changed an armed demand"));
            }
            if (!existing->granted) {
                return Result::failure(::media::ErrorInfo::wouldBlock(
                    "graph payload producer already has an armed waiter"));
            }
            for (const auto value : existing->bytes) {
                leases.push_back(MediaGraphPayloadCreditLease(m_state, value));
            }
            m_state->snapshot.reservations +=
                static_cast<std::uint64_t>(existing->bytes.size());
            m_state->waiters.erase(existing);
            nextWakeup = promoteOne(*m_state);
        } else {
            MediaGraphPayloadCreditState::Pending pending{
                producer, std::move(demand), totalBytes, wakeup, false};
            if (fits(*m_state, pending)) {
                const auto objectCount =
                    static_cast<std::uint64_t>(pending.bytes.size());
                for (const auto value : pending.bytes) {
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
            } else {
                try {
                    m_state->waiters.push_back(std::move(pending));
                } catch (const std::bad_alloc&) {
                    return Result::failure(::media::ErrorInfo::allocationFailed(
                        "graph payload waiter queue"));
                }
                ++m_state->snapshot.pressureFailures;
                return Result::failure(::media::ErrorInfo::wouldBlock(
                    "graph payload credit waiter armed at its planner hard bound"));
            }
        }
    }
    if (nextWakeup) nextWakeup->notify();
    return Result::success(std::move(leases));
}

void MediaGraphPayloadCreditLedger::cancelBlockedWaiters() noexcept
{
    std::vector<std::shared_ptr<MediaNodeWakeup>> wakeups;
    {
        std::lock_guard lock(m_state->mutex);
        m_state->cancelled = true;
        try {
            wakeups.reserve(m_state->waiters.size());
        } catch (const std::bad_alloc&) {
        }
        for (const auto& waiter : m_state->waiters) {
            if (waiter.granted) {
                const auto objects =
                    static_cast<std::uint64_t>(waiter.bytes.size());
                m_state->snapshot.currentBytes -= waiter.totalBytes;
                m_state->snapshot.currentObjects -= objects;
            }
            if (auto wakeup = waiter.wakeup.lock()) {
                try {
                    wakeups.push_back(std::move(wakeup));
                } catch (const std::bad_alloc&) {
                    wakeup->notify();
                }
            }
        }
        m_state->waiters.clear();
    }
    for (const auto& wakeup : wakeups) wakeup->notify();
}

MediaGraphPayloadCreditSnapshot
MediaGraphPayloadCreditLedger::snapshot() const noexcept
{
    std::lock_guard lock(m_state->mutex);
    return m_state->snapshot;
}

} // namespace media::ffmpeg::graph
