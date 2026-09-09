#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <algorithm>
#include <deque>
#include <limits>
#include <mutex>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

class MediaGraphPayloadAggregate final {
public:
    mutable std::mutex mutex;
    MediaGraphPayloadCreditSnapshot snapshot;
};

class MediaGraphPayloadCreditState final {
public:
    explicit MediaGraphPayloadCreditState(MediaGraphPayloadCreditPlan plan,
        std::shared_ptr<MediaGraphPayloadAggregate> aggregate)
        : plan(std::move(plan)), aggregate(std::move(aggregate)) {}

    MediaGraphPayloadCreditPlan plan;
    std::shared_ptr<MediaGraphPayloadAggregate> aggregate;
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

// Called inside the account lock. The session aggregate retains monotonic
// counters and true concurrent high-water marks after accounts are retired.
class AggregatePublication final {
public:
    explicit AggregatePublication(MediaGraphPayloadCreditState& state) noexcept
        : m_state(state), m_before(state.snapshot) {}
    ~AggregatePublication()
    {
        std::lock_guard lock(m_state.aggregate->mutex);
        auto& aggregate = m_state.aggregate->snapshot;
        const auto& after = m_state.snapshot;
        const auto adjust = [](std::uint64_t& total, std::uint64_t before,
                               std::uint64_t value) {
            if (value >= before) total += value - before;
            else total -= before - value;
        };
        adjust(aggregate.currentBytes, m_before.currentBytes, after.currentBytes);
        adjust(aggregate.currentObjects, m_before.currentObjects, after.currentObjects);
        aggregate.reservations += after.reservations - m_before.reservations;
        aggregate.releases += after.releases - m_before.releases;
        aggregate.pressureFailures += after.pressureFailures - m_before.pressureFailures;
        aggregate.highWaterBytes = (std::max)(aggregate.highWaterBytes, aggregate.currentBytes);
        aggregate.highWaterObjects = (std::max)(aggregate.highWaterObjects, aggregate.currentObjects);
    }
private:
    MediaGraphPayloadCreditState& m_state;
    MediaGraphPayloadCreditSnapshot m_before;
};

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
            if (it->granted) {
                state.snapshot.currentBytes -= it->totalBytes;
                state.snapshot.currentObjects -= it->bytes.size();
            }
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

void cancelAccount(MediaGraphPayloadCreditState& state) noexcept
{
    std::lock_guard lock(state.mutex);
    AggregatePublication publication(state);
    state.cancelled = true;
    for (const auto& waiter : state.waiters) {
        if (waiter.granted) {
            state.snapshot.currentBytes -= waiter.totalBytes;
            state.snapshot.currentObjects -= waiter.bytes.size();
        }
        if (auto wakeup = waiter.wakeup.lock()) wakeup->notify();
    }
    state.waiters.clear();
}

} // namespace

MediaGraphPayloadRetentionReservation::~MediaGraphPayloadRetentionReservation()
{
    // Owner releases this only after downstream workers and references retire.
    // Previously granted payloads remain valid even if a concurrent owner still
    // retains them; subsequent reservations respect the reduced hard bound.
    std::lock_guard lock(m_state->mutex);
    m_state->plan.maximumBytes -= m_growth.additionalBytes;
    m_state->plan.maximumObjects -= m_growth.additionalObjects;
}

::media::Result<std::shared_ptr<MediaGraphPayloadRetentionReservation>>
MediaGraphPayloadCreditLedger::reserveRetentionGrowth(MediaGraphPayloadRetentionGrowth growth)
{
    using Result = ::media::Result<std::shared_ptr<MediaGraphPayloadRetentionReservation>>;
    if (!growth.sourceAccountProducer.isValid() || !growth.additionalObjects)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Retention growth requires an existing producer and additional live-reference objects"));
    auto selected = accountForProducer(growth.sourceAccountProducer);
    if (!selected) return Result::failure(selected.error());
    auto target = std::move(selected).value();
    std::lock_guard accountsLock(m_accountsMutex);
    std::uint64_t bytes = growth.additionalBytes;
    std::uint64_t objects = growth.additionalObjects;
    const auto accumulate = [&bytes, &objects](const auto& account) {
        std::lock_guard accountLock(account->mutex);
        if (account->plan.maximumBytes > (std::numeric_limits<std::uint64_t>::max)() - bytes ||
            account->plan.maximumObjects > (std::numeric_limits<std::uint64_t>::max)() - objects)
            return false;
        bytes += account->plan.maximumBytes;
        objects += account->plan.maximumObjects;
        return true;
    };
    if (!accumulate(m_state)) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Retention growth overflows the session payload budget"));
    for (const auto& weak : m_branchAccounts)
        if (auto account = weak.lock(); account && !accumulate(account))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Retention growth overflows the session payload budget"));
    // Allocate before changing admission; no fallible operation follows publication.
    std::shared_ptr<MediaGraphPayloadRetentionReservation> reservation;
    try {
        reservation = std::shared_ptr<MediaGraphPayloadRetentionReservation>(
            new MediaGraphPayloadRetentionReservation(target, {growth.sourceAccountProducer, 0, 0}));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Retention growth reservation"));
    }
    {
        std::lock_guard lock(target->mutex);
        AggregatePublication publication(*target);
        if (target->cancelled) return Result::failure(::media::ErrorInfo::cancelled(
            "Retention growth cannot revive a retired producer account"));
        target->plan.maximumBytes += growth.additionalBytes;
        target->plan.maximumObjects += growth.additionalObjects;
        reservation->m_growth = growth;
        while (auto wakeup = promoteOne(*target)) wakeup->notify();
    }
    return Result::success(std::move(reservation));
}

MediaGraphPayloadBranchReservation::MediaGraphPayloadBranchReservation(
    std::shared_ptr<MediaGraphPayloadCreditState> state) noexcept
    : m_state(std::move(state))
{
}

MediaGraphPayloadBranchReservation::~MediaGraphPayloadBranchReservation()
{
    // Existing payload leases retain this account after admission is withdrawn.
    cancelAccount(*m_state);
}

::media::Result<std::shared_ptr<MediaGraphPayloadCreditState>>
MediaGraphPayloadCreditLedger::accountForProducer(MediaNodeId producer) const
{
    using Result = ::media::Result<std::shared_ptr<MediaGraphPayloadCreditState>>;
    std::lock_guard lock(m_accountsMutex);
    const auto found = m_producerAccounts.find(producer.value);
    if (found != m_producerAccounts.end()) {
        auto account = found->second.lock();
        if (!account) return Result::failure(::media::ErrorInfo::cancelled(
            "payload producer belongs to a retired output branch"));
        return Result::success(std::move(account));
    }
    const bool initial = std::any_of(m_plan.producers.begin(), m_plan.producers.end(),
        [producer](const auto& strategy) { return strategy.nodeId == producer; });
    if (!initial) return Result::failure(::media::ErrorInfo::invalidArgument(
        "payload producer has no admitted account"));
    return Result::success(m_state);
}

::media::Result<std::shared_ptr<MediaGraphPayloadBranchReservation>>
MediaGraphPayloadCreditLedger::extractInitialBranch(
    MediaGraphPayloadCreditPlan sharedPlan, MediaGraphPayloadCreditPlan outputPlan)
{
    using Result = ::media::Result<std::shared_ptr<MediaGraphPayloadBranchReservation>>;
    if (!sharedPlan.isCompleteAndValid() || !outputPlan.isCompleteAndValid())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "initial payload partition requires two complete planner contracts"));
    std::lock_guard accountsLock(m_accountsMutex);
    std::unique_lock initialLock(m_state->mutex);
    if (!m_branchAccounts.empty() || !m_producerAccounts.empty() || m_state->cancelled ||
        m_state->plan.maximumObjects != m_plan.maximumObjects ||
        m_state->plan.maximumBytes != m_plan.maximumBytes ||
        m_state->snapshot.currentBytes || m_state->snapshot.currentObjects ||
        m_state->snapshot.reservations || !m_state->waiters.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "initial payload partition is only legal before any reservation or execution"));
    }
    const auto sameIdentity = [](const auto& left, const auto& right) {
        return left.nodeId == right.nodeId && left.streamKind == right.streamKind &&
            left.payloadKind == right.payloadKind;
    };
    const auto equivalent = [&sameIdentity](const auto& left, const auto& right) {
        if (!sameIdentity(left, right) || left.accounting != right.accounting ||
            left.maximumReservationBytes != right.maximumReservationBytes ||
            left.frameCredit.has_value() != right.frameCredit.has_value()) return false;
        return !left.frameCredit ||
            (left.frameCredit->allocationScope == right.frameCredit->allocationScope &&
             left.frameCredit->maximumLogicalBytes == right.frameCredit->maximumLogicalBytes &&
             left.frameCredit->maximumObjectsPerAllocation == right.frameCredit->maximumObjectsPerAllocation);
    };
    if (sharedPlan.producers.size() + outputPlan.producers.size() != m_plan.producers.size())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "initial payload partition must cover the exact original producer registry"));
    for (const auto& shared : sharedPlan.producers) {
        if (std::any_of(outputPlan.producers.begin(), outputPlan.producers.end(),
                       [&shared](const auto& output) { return output.nodeId == shared.nodeId; }))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "initial payload producer node cannot belong to both accounts"));
    }
    for (const auto* plan : {&sharedPlan, &outputPlan}) {
        for (const auto& producer : plan->producers) {
            if (std::none_of(m_plan.producers.begin(), m_plan.producers.end(),
                            [&equivalent, &producer](const auto& original) {
                                return equivalent(original, producer);
                            })) return Result::failure(::media::ErrorInfo::invalidArgument(
                                "initial payload partition changed an original producer contract"));
        }
    }
    if (outputPlan.maximumBytes > (std::numeric_limits<std::uint64_t>::max)() - sharedPlan.maximumBytes ||
        outputPlan.maximumObjects > (std::numeric_limits<std::uint64_t>::max)() - sharedPlan.maximumObjects)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "initial payload partition total is not representable"));
    try {
        auto output = std::make_shared<MediaGraphPayloadCreditState>(
            std::move(outputPlan), m_state->aggregate);
        auto reservation = std::shared_ptr<MediaGraphPayloadBranchReservation>(
            new MediaGraphPayloadBranchReservation(output));
        decltype(m_producerAccounts) producers;
        for (const auto& producer : output->plan.producers)
            producers.emplace(producer.nodeId.value, output);
        decltype(m_branchAccounts) accounts;
        accounts.push_back(output);
        auto sharedStatePlan = sharedPlan;
        m_state->plan = std::move(sharedStatePlan);
        m_plan = std::move(sharedPlan);
        m_producerAccounts.swap(producers);
        m_branchAccounts.swap(accounts);
        return Result::success(std::move(reservation));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("initial payload partition"));
    }
}

::media::Result<std::shared_ptr<MediaGraphPayloadBranchReservation>>
MediaGraphPayloadCreditLedger::reserveBranch(MediaGraphPayloadCreditPlan plan)
{
    using Result = ::media::Result<std::shared_ptr<MediaGraphPayloadBranchReservation>>;
    if (!plan.isCompleteAndValid()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "branch admission requires a complete planner payload contract"));
    std::lock_guard lock(m_accountsMutex);
    for (const auto& producer : plan.producers) {
        const auto existing = m_producerAccounts.find(producer.nodeId.value);
        if ((existing != m_producerAccounts.end() && !existing->second.expired()) ||
            std::any_of(m_plan.producers.begin(), m_plan.producers.end(),
                [&producer](const auto& initial) { return initial.nodeId == producer.nodeId; })) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "branch admission cannot reuse an existing payload producer ID"));
        }
    }
    std::uint64_t bytes = 0;
    std::uint64_t objects = 0;
    const auto accumulate = [&bytes, &objects](const MediaGraphPayloadCreditPlan& account) {
        if (account.maximumBytes > (std::numeric_limits<std::uint64_t>::max)() - bytes ||
            account.maximumObjects > (std::numeric_limits<std::uint64_t>::max)() - objects) return false;
        bytes += account.maximumBytes;
        objects += account.maximumObjects;
        return true;
    };
    {
        std::lock_guard stateLock(m_state->mutex);
        if (m_state->cancelled) return Result::failure(::media::ErrorInfo::cancelled(
            "branch admission cannot revive a terminated session"));
        if (!accumulate(m_state->plan)) return Result::failure(::media::ErrorInfo::invalidArgument(
            "session admitted payload budget is not representable"));
    }
    for (const auto& weak : m_branchAccounts) {
        if (const auto account = weak.lock()) {
            std::lock_guard stateLock(account->mutex);
            if (!accumulate(account->plan)) return Result::failure(::media::ErrorInfo::invalidArgument(
                "session admitted payload budget is not representable"));
        }
    }
    if (!accumulate(plan)) return Result::failure(::media::ErrorInfo::invalidArgument(
        "branch would overflow the session admitted payload budget"));
    try {
        auto account = std::make_shared<MediaGraphPayloadCreditState>(
            std::move(plan), m_state->aggregate);
        auto reservation = std::shared_ptr<MediaGraphPayloadBranchReservation>(
            new MediaGraphPayloadBranchReservation(account));
        // Prepare both registries before publication for transactional admission.
        auto producers = m_producerAccounts;
        std::erase_if(producers, [](const auto& entry) { return entry.second.expired(); });
        for (const auto& producer : account->plan.producers)
            producers.emplace(producer.nodeId.value, account);
        auto accounts = m_branchAccounts;
        std::erase_if(accounts, [](const auto& weak) { return weak.expired(); });
        accounts.push_back(account);
        m_producerAccounts.swap(producers);
        m_branchAccounts.swap(accounts);
        return Result::success(std::move(reservation));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("branch payload admission"));
    }
}

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
        AggregatePublication publication(*m_state);
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
        AggregatePublication publication(*m_state);
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
        auto state = std::make_shared<MediaGraphPayloadCreditState>(
            plan, std::make_shared<MediaGraphPayloadAggregate>());
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
        AggregatePublication publication(*m_state);
        if (m_state->cancelled) return Result::failure(::media::ErrorInfo::cancelled(
            "graph payload credit admission was cancelled"));
        const auto objectCount = static_cast<std::uint64_t>(bytes.size());
        const bool bytePressure = totalBytes > m_state->plan.maximumBytes ||
            m_state->snapshot.currentBytes >
                m_state->plan.maximumBytes - totalBytes;
        const bool objectPressure = objectCount > m_state->plan.maximumObjects ||
            m_state->snapshot.currentObjects >
                m_state->plan.maximumObjects - objectCount;
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
    auto selected = accountForProducer(producer);
    if (!selected) return Result::failure(selected.error());
    auto account = std::move(selected).value();
    std::uint64_t totalBytes = 0;
    for (const auto value : bytes) {
        if (value > account->plan.maximumUnitBytes ||
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
        std::lock_guard lock(account->mutex);
        AggregatePublication publication(*account);
        if (account->cancelled) {
            return Result::failure(::media::ErrorInfo::cancelled(
                "graph payload credit waiters were cancelled"));
        }
        auto existing = std::find_if(
            account->waiters.begin(), account->waiters.end(),
            [producer](const auto& waiter) {
                return waiter.producer == producer;
            });
        if (existing != account->waiters.end()) {
            if (!sameDemand(*existing, bytes)) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "graph payload producer changed an armed demand"));
            }
            if (!existing->granted) {
                return Result::failure(::media::ErrorInfo::wouldBlock(
                    "graph payload producer already has an armed waiter"));
            }
            for (const auto value : existing->bytes) {
                leases.push_back(MediaGraphPayloadCreditLease(account, value));
            }
            account->snapshot.reservations +=
                static_cast<std::uint64_t>(existing->bytes.size());
            account->waiters.erase(existing);
            nextWakeup = promoteOne(*account);
        } else {
            MediaGraphPayloadCreditState::Pending pending{
                producer, std::move(demand), totalBytes, wakeup, false};
            if (fits(*account, pending)) {
                const auto objectCount =
                    static_cast<std::uint64_t>(pending.bytes.size());
                for (const auto value : pending.bytes) {
                    leases.push_back(MediaGraphPayloadCreditLease(account, value));
                }
                account->snapshot.currentBytes += totalBytes;
                account->snapshot.currentObjects += objectCount;
                account->snapshot.highWaterBytes = (std::max)(
                    account->snapshot.highWaterBytes,
                    account->snapshot.currentBytes);
                account->snapshot.highWaterObjects = (std::max)(
                    account->snapshot.highWaterObjects,
                    account->snapshot.currentObjects);
                account->snapshot.reservations += objectCount;
            } else {
                try {
                    account->waiters.push_back(std::move(pending));
                } catch (const std::bad_alloc&) {
                    return Result::failure(::media::ErrorInfo::allocationFailed(
                        "graph payload waiter queue"));
                }
                ++account->snapshot.pressureFailures;
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
    std::lock_guard accountsLock(m_accountsMutex);
    cancelAccount(*m_state);
    for (const auto& weak : m_branchAccounts) {
        if (const auto account = weak.lock()) cancelAccount(*account);
    }
}

MediaGraphPayloadCreditSnapshot
MediaGraphPayloadCreditLedger::snapshot() const noexcept
{
    std::lock_guard accountsLock(m_accountsMutex);
    MediaGraphPayloadCreditSnapshot result;
    {
        std::lock_guard aggregateLock(m_state->aggregate->mutex);
        result = m_state->aggregate->snapshot;
    }
    {
        std::lock_guard stateLock(m_state->mutex);
        result.admittedMaximumBytes = m_state->plan.maximumBytes;
        result.admittedMaximumObjects = m_state->plan.maximumObjects;
    }
    for (const auto& weak : m_branchAccounts) {
        if (const auto account = weak.lock()) {
            std::lock_guard stateLock(account->mutex);
            result.admittedMaximumBytes += account->plan.maximumBytes;
            result.admittedMaximumObjects += account->plan.maximumObjects;
        }
    }
    return result;
}

} // namespace media::ffmpeg::graph
