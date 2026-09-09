#pragma once

#include "internal/graph/model/MediaDatagramServiceScopeContract.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

// One controller's managed traffic, sharing its clock domain. Socket ownership
// remains on each sender worker; only actual submission eligibility is shared.
class MediaDatagramServiceScopeArbiter final
    : public std::enable_shared_from_this<MediaDatagramServiceScopeArbiter> {
public:
    struct Snapshot final {
        std::uint64_t chargedDatagrams = 0;
        std::uint64_t chargedWireBytes = 0;
        std::uint64_t waitingMembers = 0;
        bool failed = false;
    };
    Snapshot snapshot() const noexcept;
    class Member;
    class SubmitGuard final {
    public:
        SubmitGuard(SubmitGuard&&) noexcept;
        SubmitGuard& operator=(SubmitGuard&&) = delete;
        ~SubmitGuard();
        // Call immediately before entering the transport submission API.
        void submissionStarted() noexcept { m_submissionStarted = true; }
        ::media::Status complete(std::uint64_t submittedWireBytes,
                                  MediaRunningTime completedAt) noexcept;
        void poison(::media::ErrorInfo error) noexcept;
    private:
        friend class Member;
        SubmitGuard(std::shared_ptr<Member> member, std::uint64_t bytes,
                    MediaRunningTime startedAt) noexcept;
        std::shared_ptr<Member> m_member;
        std::uint64_t m_bytes;
        MediaRunningTime m_startedAt;
        bool m_submissionStarted = false;
    };
    struct Attempt final {
        std::optional<SubmitGuard> permit;
        std::optional<MediaRunningTime> notBefore;
    };
    class Member final : public std::enable_shared_from_this<Member> {
    public:
        ~Member();
        ::media::Result<Attempt> tryAcquire(MediaRunningTime now,
            std::uint64_t wireBytes, MediaRunningTime immutableDeadline);
    private:
        friend class MediaDatagramServiceScopeArbiter;
        friend class SubmitGuard;
        Member(std::shared_ptr<MediaDatagramServiceScopeArbiter> owner,
               std::shared_ptr<MediaNodeWakeup> wakeup) noexcept;
        std::shared_ptr<MediaDatagramServiceScopeArbiter> m_owner;
        std::shared_ptr<MediaNodeWakeup> m_wakeup;
        Member* m_previous = nullptr;
        Member* m_next = nullptr;
        bool m_queued = false;
    };
    static ::media::Result<std::shared_ptr<MediaDatagramServiceScopeArbiter>> create(
        MediaDatagramServiceScopeContract contract,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> clockDomain);
    ::media::Result<std::shared_ptr<Member>> join(
        std::shared_ptr<MediaNodeWakeup> wakeup,
        const MediaClockDomainIdentity& clockDomain);
    const MediaDatagramServiceScopeContract& contract() const noexcept { return m_contract; }
private:
    MediaDatagramServiceScopeArbiter(MediaDatagramServiceScopeContract contract,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> clockDomain);
    void unlinkLocked(Member& member) noexcept;
    void notifyWaitersLocked() noexcept;
    MediaDatagramServiceScopeContract m_contract;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_clockAuthority;
    MediaClockDomainIdentity m_clockDomain;
    mutable std::mutex m_mutex;
    Snapshot m_snapshot;
    Member* m_head = nullptr;
    Member* m_tail = nullptr;
    Member* m_active = nullptr;
    std::optional<MediaRunningTime> m_theoreticalArrival;
    std::optional<::media::ErrorInfo> m_failure;
    ::media::ErrorInfo m_abandonedSubmissionFailure;
};

} // namespace media::ffmpeg::graph
