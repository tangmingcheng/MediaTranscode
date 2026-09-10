#include "internal/graph/runtime/network/MediaDatagramServiceScopeArbiter.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <utility>
#include <limits>
#include <type_traits>

namespace media::ffmpeg::graph {

static_assert(std::is_nothrow_move_constructible_v<::media::ErrorInfo>);

MediaDatagramServiceScopeArbiter::MediaDatagramServiceScopeArbiter(
    MediaDatagramServiceScopeContract contract,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> clockDomain)
    : m_contract(std::move(contract)), m_clockAuthority(std::move(clockDomain)), m_clockDomain(m_clockAuthority->clockDomainIdentity()),
      m_abandonedSubmissionFailure(::media::ErrorInfo::internalError(
          "shared datagram submission ended without authoritative completion")) {}

::media::Result<std::shared_ptr<MediaDatagramServiceScopeArbiter>>
MediaDatagramServiceScopeArbiter::create(MediaDatagramServiceScopeContract contract,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> clockDomain)
{
    using Result = ::media::Result<std::shared_ptr<MediaDatagramServiceScopeArbiter>>;
    if (!clockDomain || !clockDomain->clockDomainIdentity().valid() || contract.scopeId.empty() || contract.coverageAuthority.empty() ||
        contract.capacityAuthority.empty() || !contract.capacityWireBytesPerSecond)
        return Result::failure(::media::ErrorInfo::invalidArgument("shared datagram scope requires complete planning and clock facts"));
    return Result::success(std::shared_ptr<MediaDatagramServiceScopeArbiter>(
        new MediaDatagramServiceScopeArbiter(std::move(contract), std::move(clockDomain))));
}

MediaDatagramServiceScopeArbiter::Member::Member(
    std::shared_ptr<MediaDatagramServiceScopeArbiter> owner,
    std::shared_ptr<MediaNodeWakeup> wakeup) noexcept
    : m_owner(std::move(owner)), m_wakeup(std::move(wakeup)) {}

::media::Result<std::shared_ptr<MediaDatagramServiceScopeArbiter::Member>>
MediaDatagramServiceScopeArbiter::join(std::shared_ptr<MediaNodeWakeup> wakeup,
    const MediaClockDomainIdentity& clockDomain)
{
    using Result = ::media::Result<std::shared_ptr<Member>>;
    if (!wakeup || clockDomain != m_clockDomain)
        return Result::failure(::media::ErrorInfo::invalidArgument("shared datagram scope clock domain differs"));
    return Result::success(std::shared_ptr<Member>(new Member(shared_from_this(), std::move(wakeup))));
}

void MediaDatagramServiceScopeArbiter::notifyWaitersLocked() noexcept
{
    if (m_failure) {
        for (auto* member = m_head; member; member = member->m_next) member->m_wakeup->notify();
    } else if (m_head) m_head->m_wakeup->notify();
}

void MediaDatagramServiceScopeArbiter::unlinkLocked(Member& member) noexcept
{
    if (!member.m_queued) return;
    if (member.m_previous) member.m_previous->m_next = member.m_next;
    else m_head = member.m_next;
    if (member.m_next) member.m_next->m_previous = member.m_previous;
    else m_tail = member.m_previous;
    member.m_previous = member.m_next = nullptr;
    member.m_queued = false;
    --m_snapshot.waitingMembers;
}

MediaDatagramServiceScopeArbiter::Member::~Member()
{
    std::lock_guard lock(m_owner->m_mutex);
    m_owner->unlinkLocked(*this);
    m_owner->notifyWaitersLocked();
}

::media::Result<MediaDatagramServiceScopeArbiter::Attempt>
MediaDatagramServiceScopeArbiter::Member::tryAcquire(
    MediaRunningTime now, std::uint64_t wireBytes, MediaRunningTime immutableDeadline)
{
    using Result = ::media::Result<Attempt>;
    auto& owner = *m_owner;
    std::lock_guard lock(owner.m_mutex);
    if (owner.m_failure) return Result::failure(*owner.m_failure);
    if (!wireBytes || now.nanoseconds() < 0 || now > immutableDeadline || owner.m_active == this)
        return Result::failure(::media::ErrorInfo::invalidArgument("invalid shared datagram submission request"));
    if (!m_queued) {
        m_previous = owner.m_tail;
        if (owner.m_tail) owner.m_tail->m_next = this;
        else owner.m_head = this;
        owner.m_tail = this;
        m_queued = true;
        ++owner.m_snapshot.waitingMembers;
    }
    if (owner.m_active || owner.m_head != this)
        return Result::success(Attempt{std::nullopt, immutableDeadline});
    if (owner.m_theoreticalArrival && now < *owner.m_theoreticalArrival) {
        if (*owner.m_theoreticalArrival > immutableDeadline) {
            owner.unlinkLocked(*this);
            owner.notifyWaitersLocked();
            return Result::failure(::media::ErrorInfo::ioFailure("shared datagram service cannot meet immutable deadline"));
        }
        return Result::success(Attempt{std::nullopt, owner.m_theoreticalArrival});
    }
    owner.unlinkLocked(*this);
    owner.m_active = this;
    return Result::success(Attempt{SubmitGuard(shared_from_this(), wireBytes, now), std::nullopt});
}

MediaDatagramServiceScopeArbiter::SubmitGuard::SubmitGuard(
    std::shared_ptr<Member> member, std::uint64_t bytes, MediaRunningTime startedAt) noexcept
    : m_member(std::move(member)), m_bytes(bytes), m_startedAt(startedAt) {}
MediaDatagramServiceScopeArbiter::SubmitGuard::SubmitGuard(SubmitGuard&& other) noexcept
    : m_member(std::move(other.m_member)), m_bytes(other.m_bytes), m_startedAt(other.m_startedAt),
      m_submissionStarted(other.m_submissionStarted) {}
MediaDatagramServiceScopeArbiter::SubmitGuard::~SubmitGuard()
{
    if (!m_member) return;
    auto& owner = *m_member->m_owner;
    std::lock_guard lock(owner.m_mutex);
    // Once the syscall may have run, cancellation cannot refund an unknown
    // service amount. Move the preallocated error without destructor allocation.
    if (m_submissionStarted && !owner.m_failure)
        owner.m_failure.emplace(std::move(owner.m_abandonedSubmissionFailure));
    owner.m_active = nullptr;
    owner.notifyWaitersLocked();
}

::media::Status MediaDatagramServiceScopeArbiter::SubmitGuard::complete(
    std::uint64_t submittedWireBytes, MediaRunningTime completedAt) noexcept
{
    if (!m_member) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("shared datagram permit already completed"));
    auto member = std::move(m_member);
    auto& owner = *member->m_owner;
    std::lock_guard lock(owner.m_mutex);
    ::media::Status status = ::media::Status::success();
    if (!m_submissionStarted || submittedWireBytes > m_bytes || completedAt < m_startedAt) {
        status = ::media::Status::failure(::media::ErrorInfo::invalidArgument("shared datagram completion differs from permit"));
    } else if (submittedWireBytes > (std::numeric_limits<std::uint64_t>::max)() - owner.m_snapshot.chargedWireBytes ||
               (submittedWireBytes && owner.m_snapshot.chargedDatagrams == (std::numeric_limits<std::uint64_t>::max)())) {
        status = ::media::Status::failure(::media::ErrorInfo::internalError("shared datagram scope telemetry overflow"));
    } else if (submittedWireBytes) {
        owner.m_snapshot.chargedWireBytes += submittedWireBytes;
        ++owner.m_snapshot.chargedDatagrams;
        auto duration = MediaCheckedArithmetic::ceilDurationNanoseconds(
            submittedWireBytes, owner.m_contract.capacityWireBytesPerSecond,
            "shared datagram peak service increment");
        if (!duration) status = ::media::Status::failure(duration.error());
        else {
            auto next = completedAt.checkedAdd(MediaRunningTime::fromNanoseconds(duration.value()));
            if (!next) status = ::media::Status::failure(next.error());
            else owner.m_theoreticalArrival = next.value();
        }
    }
    if (!status && !owner.m_failure) owner.m_failure = status.error();
    owner.m_active = nullptr;
    owner.notifyWaitersLocked();
    return status;
}

void MediaDatagramServiceScopeArbiter::SubmitGuard::poison(::media::ErrorInfo error) noexcept
{
    if (!m_member) return;
    auto& owner = *m_member->m_owner;
    std::lock_guard lock(owner.m_mutex);
    if (!owner.m_failure) owner.m_failure = std::move(error);
    owner.notifyWaitersLocked();
}

MediaDatagramServiceScopeArbiter::Snapshot MediaDatagramServiceScopeArbiter::snapshot() const noexcept
{
    std::lock_guard lock(m_mutex);
    auto result = m_snapshot;
    result.failed = m_failure.has_value();
    return result;
}

} // namespace media::ffmpeg::graph
