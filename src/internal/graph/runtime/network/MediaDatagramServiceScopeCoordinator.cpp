#include "internal/graph/runtime/network/MediaDatagramServiceScopeCoordinator.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaDatagramServiceScopeState final {
    struct Member final {
        MediaDatagramPacingContract contract;
        bool reservationActiveOrWaiting = false;
    };
    struct ActiveReservation final {
        std::uint64_t memberId;
        std::uint64_t reservationId;
    };

    explicit MediaDatagramServiceScopeState(
        std::string selectedScopeId,
        std::uint64_t selectedMaximumWireBytesPerSecond) noexcept
        : scopeId(std::move(selectedScopeId)),
          maximumWireBytesPerSecond(selectedMaximumWireBytesPerSecond)
    {
        telemetry.maximumWireBytesPerSecond = maximumWireBytesPerSecond;
    }

    std::string scopeId;
    std::uint64_t maximumWireBytesPerSecond;
    std::uint64_t admittedWireBytesPerSecond = 0;
    std::uint64_t nextMemberId = 1;
    std::uint64_t nextReservationId = 1;
    std::unordered_map<std::uint64_t, Member> members;
    std::deque<std::uint64_t> readyMembers;
    std::optional<ActiveReservation> activeReservation;
    std::optional<std::chrono::steady_clock::time_point>
        theoreticalArrivalTime;
    std::optional<::media::ErrorInfo> terminalFailure;
    MediaDatagramServiceScopeTelemetry telemetry;
    mutable std::mutex mutex;
    std::condition_variable_any changed;
};

namespace {

struct ServiceScopeRegistry final {
    std::mutex mutex;
    // Service debt survives zero-member gaps.  Process teardown is the
    // explicit reset boundary for one provisioned egress scope.
    std::unordered_map<
        std::string, std::shared_ptr<MediaDatagramServiceScopeState>> scopes;
};

ServiceScopeRegistry& serviceScopeRegistry()
{
    static ServiceScopeRegistry registry;
    return registry;
}

void increment(std::uint64_t& counter,
               MediaDatagramServiceScopeTelemetry& telemetry) noexcept
{
    if (counter == (std::numeric_limits<std::uint64_t>::max)()) {
        telemetry.counterSaturated = true;
        return;
    }
    ++counter;
}

void eraseReadyMember(MediaDatagramServiceScopeState& state,
                      std::uint64_t memberId) noexcept
{
    const auto found = std::find(
        state.readyMembers.begin(), state.readyMembers.end(), memberId);
    if (found != state.readyMembers.end()) state.readyMembers.erase(found);
}

::media::ErrorInfo saturatedTelemetryError()
{
    return ::media::ErrorInfo::internalError(
        "Datagram service-scope telemetry saturated");
}

} // namespace

MediaDatagramServiceScopeReservation::MediaDatagramServiceScopeReservation(
    std::shared_ptr<MediaDatagramServiceScopeState> state,
    std::uint64_t memberId,
    std::uint64_t reservationId,
    std::chrono::steady_clock::time_point notBefore,
    std::chrono::steady_clock::time_point notAfter,
    std::chrono::nanoseconds serviceDuration) noexcept
    : m_state(std::move(state)),
      m_memberId(memberId),
      m_reservationId(reservationId),
      m_notBefore(notBefore),
      m_notAfter(notAfter),
      m_serviceDuration(serviceDuration)
{
}

MediaDatagramServiceScopeReservation::~MediaDatagramServiceScopeReservation()
    noexcept
{
    cancel();
}

MediaDatagramServiceScopeReservation::MediaDatagramServiceScopeReservation(
    MediaDatagramServiceScopeReservation&& other) noexcept
    : m_state(std::move(other.m_state)),
      m_memberId(std::exchange(other.m_memberId, 0)),
      m_reservationId(std::exchange(other.m_reservationId, 0)),
      m_notBefore(other.m_notBefore),
      m_notAfter(other.m_notAfter),
      m_serviceDuration(other.m_serviceDuration)
{
}

MediaDatagramServiceScopeReservation&
MediaDatagramServiceScopeReservation::operator=(
    MediaDatagramServiceScopeReservation&& other) noexcept
{
    if (this == &other) return *this;
    cancel();
    m_state = std::move(other.m_state);
    m_memberId = std::exchange(other.m_memberId, 0);
    m_reservationId = std::exchange(other.m_reservationId, 0);
    m_notBefore = other.m_notBefore;
    m_notAfter = other.m_notAfter;
    m_serviceDuration = other.m_serviceDuration;
    return *this;
}

void MediaDatagramServiceScopeReservation::cancel() noexcept
{
    if (!m_state) return;
    try {
        std::lock_guard lock(m_state->mutex);
        if (m_state->activeReservation &&
            m_state->activeReservation->memberId == m_memberId &&
            m_state->activeReservation->reservationId == m_reservationId) {
            m_state->activeReservation.reset();
            const auto member = m_state->members.find(m_memberId);
            if (member != m_state->members.end()) {
                member->second.reservationActiveOrWaiting = false;
            }
            increment(m_state->telemetry.cancelledReservations,
                      m_state->telemetry);
            m_state->changed.notify_all();
        }
    } catch (...) {
    }
    m_state.reset();
    m_memberId = 0;
    m_reservationId = 0;
}

::media::Status MediaDatagramServiceScopeReservation::markSubmitted(
    std::chrono::steady_clock::time_point submitStartedAt,
    std::chrono::steady_clock::time_point submitCompletedAt)
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram service-scope reservation is not active"));
    }
    std::unique_lock lock(m_state->mutex);
    if (!m_state->activeReservation ||
        m_state->activeReservation->memberId != m_memberId ||
        m_state->activeReservation->reservationId != m_reservationId ||
        submitStartedAt < m_notBefore || submitStartedAt >= m_notAfter ||
        submitCompletedAt < submitStartedAt ||
        submitCompletedAt >= m_notAfter ||
        m_serviceDuration <= std::chrono::nanoseconds::zero() ||
        submitCompletedAt >
            std::chrono::steady_clock::time_point::max() - m_serviceDuration) {
        auto error = ::media::ErrorInfo::ioFailure(
            "submitted Datagram violated its aggregate service reservation");
        if (m_state->activeReservation &&
            m_state->activeReservation->memberId == m_memberId) {
            m_state->activeReservation.reset();
        }
        const auto member = m_state->members.find(m_memberId);
        if (member != m_state->members.end()) {
            member->second.reservationActiveOrWaiting = false;
        }
        m_state->terminalFailure = error;
        m_state->changed.notify_all();
        lock.unlock();
        m_state.reset();
        m_memberId = 0;
        m_reservationId = 0;
        return ::media::Status::failure(std::move(error));
    }
    m_state->theoreticalArrivalTime = submitCompletedAt + m_serviceDuration;
    m_state->activeReservation.reset();
    const auto member = m_state->members.find(m_memberId);
    if (member != m_state->members.end()) {
        member->second.reservationActiveOrWaiting = false;
    }
    increment(m_state->telemetry.submittedDatagrams, m_state->telemetry);
    const bool saturated = m_state->telemetry.counterSaturated;
    if (saturated) m_state->terminalFailure = saturatedTelemetryError();
    m_state->changed.notify_all();
    lock.unlock();
    m_state.reset();
    m_memberId = 0;
    m_reservationId = 0;
    return saturated
        ? ::media::Status::failure(saturatedTelemetryError())
        : ::media::Status::success();
}

::media::Status MediaDatagramServiceScopeReservation::markAmbiguous(
    ::media::ErrorInfo cause)
{
    if (!m_state || cause.ok()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "ambiguous Datagram scope submission requires active causality"));
    }
    std::unique_lock lock(m_state->mutex);
    if (!m_state->activeReservation ||
        m_state->activeReservation->memberId != m_memberId ||
        m_state->activeReservation->reservationId != m_reservationId) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "ambiguous Datagram scope submission has no active reservation"));
    }
    m_state->activeReservation.reset();
    const auto member = m_state->members.find(m_memberId);
    if (member != m_state->members.end()) {
        member->second.reservationActiveOrWaiting = false;
    }
    increment(m_state->telemetry.ambiguousSubmissions, m_state->telemetry);
    m_state->terminalFailure = std::move(cause);
    m_state->changed.notify_all();
    lock.unlock();
    m_state.reset();
    m_memberId = 0;
    m_reservationId = 0;
    return ::media::Status::success();
}

MediaDatagramServiceScopeMembership::~MediaDatagramServiceScopeMembership()
    noexcept
{
    if (!m_state) return;
    try {
        std::lock_guard lock(m_state->mutex);
        const auto member = m_state->members.find(m_memberId);
        if (member != m_state->members.end()) {
            eraseReadyMember(*m_state, m_memberId);
            if (m_state->activeReservation &&
                m_state->activeReservation->memberId == m_memberId) {
                m_state->activeReservation.reset();
                increment(m_state->telemetry.cancelledReservations,
                          m_state->telemetry);
            }
            m_state->admittedWireBytesPerSecond -=
                member->second.contract.wireBytesPerSecond;
            m_state->members.erase(member);
            m_state->telemetry.activeMembers =
                static_cast<std::uint64_t>(m_state->members.size());
            m_state->telemetry.admittedWireBytesPerSecond =
                m_state->admittedWireBytesPerSecond;
            m_state->changed.notify_all();
        }
    } catch (...) {
    }
}

::media::Result<std::unique_ptr<MediaDatagramServiceScopeMembership>>
MediaDatagramServiceScopeMembership::join(
    MediaDatagramPacingContract contract)
{
    using Result = ::media::Result<
        std::unique_ptr<MediaDatagramServiceScopeMembership>>;
    auto valid = validateMediaDatagramPacingContract(contract);
    if (!valid) return Result::failure(valid.error());

    try {
        auto& registry = serviceScopeRegistry();
        auto membership =
            std::unique_ptr<MediaDatagramServiceScopeMembership>(
                new (std::nothrow) MediaDatagramServiceScopeMembership());
        if (!membership) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "Datagram service-scope membership"));
        }

        std::lock_guard registryLock(registry.mutex);
        std::shared_ptr<MediaDatagramServiceScopeState> state;
        const auto found = registry.scopes.find(contract.serviceScopeId);
        if (found != registry.scopes.end()) state = found->second;
        const auto publishesNewScope = !state;
        if (publishesNewScope) {
            state = std::make_shared<MediaDatagramServiceScopeState>(
                contract.serviceScopeId,
                contract.maximumWireBytesPerSecond);
        }

        std::lock_guard scopeLock(state->mutex);
        if (state->terminalFailure) {
            return Result::failure(*state->terminalFailure);
        }
        if (state->maximumWireBytesPerSecond !=
            contract.maximumWireBytesPerSecond) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Datagram service scope has conflicting managed capacities"));
        }
        for (const auto& [memberId, member] : state->members) {
            if (member.contract.sessionKey == contract.sessionKey) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Datagram session already joined its service scope"));
            }
            (void)memberId;
        }
        if (contract.wireBytesPerSecond >
            state->maximumWireBytesPerSecond -
                state->admittedWireBytesPerSecond) {
            return Result::failure(::media::ErrorInfo::ioFailure(
                "aggregate Datagram pacing demand exceeds managed service capacity"));
        }
        if (state->nextMemberId ==
            (std::numeric_limits<std::uint64_t>::max)()) {
            return Result::failure(::media::ErrorInfo::internalError(
                "Datagram service-scope member identity exhausted"));
        }
        const auto memberId = state->nextMemberId;
        const auto admittedWireBytesPerSecond = contract.wireBytesPerSecond;
        const auto [member, inserted] = state->members.emplace(
            memberId,
            MediaDatagramServiceScopeState::Member{std::move(contract)});
        if (!inserted) {
            return Result::failure(::media::ErrorInfo::internalError(
                "Datagram service-scope member identity collided"));
        }
        if (publishesNewScope) {
            const auto [scope, scopeInserted] = registry.scopes.emplace(
                state->scopeId, state);
            if (!scopeInserted) {
                state->members.erase(member);
                return Result::failure(::media::ErrorInfo::internalError(
                    "Datagram service scope publication collided"));
            }
            (void)scope;
        }
        ++state->nextMemberId;
        state->admittedWireBytesPerSecond +=
            admittedWireBytesPerSecond;
        state->telemetry.activeMembers =
            static_cast<std::uint64_t>(state->members.size());
        state->telemetry.highWaterMembers = (std::max)(
            state->telemetry.highWaterMembers,
            state->telemetry.activeMembers);
        state->telemetry.admittedWireBytesPerSecond =
            state->admittedWireBytesPerSecond;
        membership->m_state = std::move(state);
        membership->m_memberId = memberId;
        return Result::success(std::move(membership));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Datagram service-scope membership"));
    }
}

::media::Status MediaDatagramServiceScopeMembership::rebind(
    MediaDatagramPacingContract contract)
{
    auto valid = validateMediaDatagramPacingContract(contract);
    if (!valid) return valid;
    std::lock_guard lock(m_state->mutex);
    if (m_state->terminalFailure) {
        return ::media::Status::failure(*m_state->terminalFailure);
    }
    const auto member = m_state->members.find(m_memberId);
    if (member == m_state->members.end() ||
        member->second.reservationActiveOrWaiting ||
        contract.generation <= member->second.contract.generation ||
        !mediaDatagramPacingContractsDescribeSamePersistentService(
            member->second.contract, contract)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram service-scope rebind changes active persistent service"));
    }
    member->second.contract = std::move(contract);
    return ::media::Status::success();
}

::media::Result<MediaDatagramServiceScopeReservation>
MediaDatagramServiceScopeMembership::reserve(
    std::uint64_t wireBytes,
    std::chrono::steady_clock::time_point immutableSubmitDeadline,
    std::stop_token stopToken)
{
    using Result =
        ::media::Result<MediaDatagramServiceScopeReservation>;
    auto durationNanoseconds = MediaCheckedArithmetic::ceilDurationNanoseconds(
        wireBytes, m_state->maximumWireBytesPerSecond,
        "aggregate Datagram service-scope increment");
    if (!durationNanoseconds || durationNanoseconds.value() <= 0) {
        return Result::failure(
            durationNanoseconds
                ? ::media::ErrorInfo::invalidArgument(
                      "aggregate Datagram service increment is not positive")
                : durationNanoseconds.error());
    }
    const auto serviceDuration =
        std::chrono::nanoseconds(durationNanoseconds.value());
    const auto initialNow = std::chrono::steady_clock::now();
    if (wireBytes == 0 || immutableSubmitDeadline <= initialNow) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "aggregate Datagram reservation has no immutable submit window"));
    }

    std::unique_lock lock(m_state->mutex);
    auto member = m_state->members.find(m_memberId);
    if (member == m_state->members.end() ||
        member->second.reservationActiveOrWaiting) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram service-scope member already has outstanding work"));
    }
    if (m_state->terminalFailure) {
        return Result::failure(*m_state->terminalFailure);
    }
    try {
        member->second.reservationActiveOrWaiting = true;
        m_state->readyMembers.push_back(m_memberId);
    } catch (const std::bad_alloc&) {
        member->second.reservationActiveOrWaiting = false;
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Datagram aggregate ready-member queue"));
    }
    if (m_state->activeReservation ||
        m_state->readyMembers.front() != m_memberId) {
        increment(m_state->telemetry.contentionWaits, m_state->telemetry);
    }
    const auto ready = [this] {
        return m_state->terminalFailure.has_value() ||
               (!m_state->activeReservation &&
                !m_state->readyMembers.empty() &&
                m_state->readyMembers.front() == m_memberId);
    };
    const bool acquired =
        m_state->changed.wait_until(
            lock, stopToken, immutableSubmitDeadline, ready);
    member = m_state->members.find(m_memberId);
    if (member == m_state->members.end()) {
        eraseReadyMember(*m_state, m_memberId);
        m_state->changed.notify_all();
        return Result::failure(::media::ErrorInfo::cancelled(
            "Datagram service-scope membership ended while waiting"));
    }
    if (!acquired || m_state->terminalFailure) {
        eraseReadyMember(*m_state, m_memberId);
        member->second.reservationActiveOrWaiting = false;
        m_state->changed.notify_all();
        if (m_state->terminalFailure) {
            return Result::failure(*m_state->terminalFailure);
        }
        if (stopToken.stop_requested()) {
            return Result::failure(::media::ErrorInfo::cancelled(
                "aggregate Datagram reservation wait was stopped"));
        }
        increment(m_state->telemetry.deadlineRejections,
                  m_state->telemetry);
        return Result::failure(::media::ErrorInfo::ioFailure(
            "aggregate Datagram reservation reached immutable deadline"));
    }

    const auto now = std::chrono::steady_clock::now();
    const auto notBefore = m_state->theoreticalArrivalTime
        ? (std::max)(now, *m_state->theoreticalArrivalTime)
        : now;
    if (notBefore >= immutableSubmitDeadline) {
        m_state->readyMembers.pop_front();
        member->second.reservationActiveOrWaiting = false;
        increment(m_state->telemetry.deadlineRejections,
                  m_state->telemetry);
        m_state->changed.notify_all();
        return Result::failure(::media::ErrorInfo::ioFailure(
            "aggregate Datagram service debt exceeds immutable deadline"));
    }
    if (m_state->nextReservationId ==
        (std::numeric_limits<std::uint64_t>::max)()) {
        m_state->readyMembers.pop_front();
        member->second.reservationActiveOrWaiting = false;
        m_state->terminalFailure = ::media::ErrorInfo::internalError(
            "Datagram service-scope reservation identity exhausted");
        m_state->changed.notify_all();
        return Result::failure(*m_state->terminalFailure);
    }
    const auto reservationId = m_state->nextReservationId++;
    m_state->readyMembers.pop_front();
    m_state->activeReservation =
        MediaDatagramServiceScopeState::ActiveReservation{
            m_memberId, reservationId};
    increment(m_state->telemetry.reservedDatagrams, m_state->telemetry);
    if (m_state->telemetry.counterSaturated) {
        m_state->activeReservation.reset();
        member->second.reservationActiveOrWaiting = false;
        m_state->terminalFailure = saturatedTelemetryError();
        m_state->changed.notify_all();
        return Result::failure(*m_state->terminalFailure);
    }
    return Result::success(MediaDatagramServiceScopeReservation(
        m_state, m_memberId, reservationId, notBefore,
        immutableSubmitDeadline,
        serviceDuration));
}

MediaDatagramServiceScopeTelemetry
MediaDatagramServiceScopeMembership::telemetry() const noexcept
{
    try {
        std::lock_guard lock(m_state->mutex);
        return m_state->telemetry;
    } catch (...) {
        auto telemetry = MediaDatagramServiceScopeTelemetry{};
        telemetry.counterSaturated = true;
        return telemetry;
    }
}

} // namespace media::ffmpeg::graph
