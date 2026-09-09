#pragma once

#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "internal/graph/model/MediaGraphPayloadRetentionGrowth.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace media::ffmpeg::graph {

struct MediaGraphPayloadCreditSnapshot final {
    std::uint64_t admittedMaximumBytes = 0;
    std::uint64_t admittedMaximumObjects = 0;
    std::uint64_t currentBytes = 0;
    std::uint64_t currentObjects = 0;
    std::uint64_t highWaterBytes = 0;
    std::uint64_t highWaterObjects = 0;
    std::uint64_t reservations = 0;
    std::uint64_t releases = 0;
    std::uint64_t pressureFailures = 0;
};

class MediaGraphPayloadCreditState;

class MediaGraphPayloadRetentionReservation final {
public:
    ~MediaGraphPayloadRetentionReservation();
    MediaGraphPayloadRetentionReservation(const MediaGraphPayloadRetentionReservation&) = delete;
    MediaGraphPayloadRetentionReservation& operator=(const MediaGraphPayloadRetentionReservation&) = delete;
private:
    friend class MediaGraphPayloadCreditLedger;
    MediaGraphPayloadRetentionReservation(std::shared_ptr<MediaGraphPayloadCreditState> state,
        MediaGraphPayloadRetentionGrowth growth) noexcept
        : m_state(std::move(state)), m_growth(growth) {}
    std::shared_ptr<MediaGraphPayloadCreditState> m_state;
    MediaGraphPayloadRetentionGrowth m_growth;
};

class MediaGraphPayloadBranchReservation final {
public:
    ~MediaGraphPayloadBranchReservation();
    MediaGraphPayloadBranchReservation(const MediaGraphPayloadBranchReservation&) = delete;
    MediaGraphPayloadBranchReservation& operator=(const MediaGraphPayloadBranchReservation&) = delete;
private:
    friend class MediaGraphPayloadCreditLedger;
    explicit MediaGraphPayloadBranchReservation(
        std::shared_ptr<MediaGraphPayloadCreditState> state) noexcept;
    std::shared_ptr<MediaGraphPayloadCreditState> m_state;
};

class MediaGraphPayloadCreditLease final {
public:
    MediaGraphPayloadCreditLease() noexcept = default;
    ~MediaGraphPayloadCreditLease();
    MediaGraphPayloadCreditLease(MediaGraphPayloadCreditLease&&) noexcept;
    MediaGraphPayloadCreditLease& operator=(
        MediaGraphPayloadCreditLease&&) noexcept;
    MediaGraphPayloadCreditLease(const MediaGraphPayloadCreditLease&) = delete;
    MediaGraphPayloadCreditLease& operator=(
        const MediaGraphPayloadCreditLease&) = delete;

    ::media::Status shrinkTo(std::uint64_t bytes) noexcept;
    std::uint64_t bytes() const noexcept { return m_bytes; }
    explicit operator bool() const noexcept { return m_state != nullptr; }

private:
    friend class MediaGraphPayloadCreditLedger;
    MediaGraphPayloadCreditLease(
        std::shared_ptr<MediaGraphPayloadCreditState> state,
        std::uint64_t bytes) noexcept;
    void release() noexcept;

    std::shared_ptr<MediaGraphPayloadCreditState> m_state;
    std::uint64_t m_bytes = 0;
};

class MediaGraphPayloadCreditLedger final {
public:
    static ::media::Result<std::shared_ptr<MediaGraphPayloadCreditLedger>>
    create(MediaGraphPayloadCreditPlan plan);

    ::media::Result<MediaGraphPayloadCreditLease> tryReserve(
        std::uint64_t bytes) noexcept;
    ::media::Result<std::vector<MediaGraphPayloadCreditLease>> tryReserveBatch(
        std::span<const std::uint64_t> bytes) noexcept;
    ::media::Result<std::vector<MediaGraphPayloadCreditLease>> tryReserveOrArm(
        MediaNodeId producer,
        std::span<const std::uint64_t> bytes,
        std::shared_ptr<MediaNodeWakeup> wakeup) noexcept;
    ::media::Result<std::shared_ptr<MediaGraphPayloadBranchReservation>> reserveBranch(
        MediaGraphPayloadCreditPlan plan);
    ::media::Result<std::shared_ptr<MediaGraphPayloadBranchReservation>> extractInitialBranch(
        MediaGraphPayloadCreditPlan sharedPlan,
        MediaGraphPayloadCreditPlan outputPlan);
    ::media::Result<std::shared_ptr<MediaGraphPayloadRetentionReservation>> reserveRetentionGrowth(
        MediaGraphPayloadRetentionGrowth growth);
    void cancelBlockedWaiters() noexcept;
    MediaGraphPayloadCreditSnapshot snapshot() const noexcept;
    const MediaGraphPayloadCreditPlan& plan() const noexcept { return m_plan; }

private:
    MediaGraphPayloadCreditLedger(
        MediaGraphPayloadCreditPlan plan,
        std::shared_ptr<MediaGraphPayloadCreditState> state) noexcept;

    ::media::Result<std::shared_ptr<MediaGraphPayloadCreditState>> accountForProducer(
        MediaNodeId producer) const;
    MediaGraphPayloadCreditPlan m_plan;
    std::shared_ptr<MediaGraphPayloadCreditState> m_state;
    mutable std::mutex m_accountsMutex;
    std::vector<std::weak_ptr<MediaGraphPayloadCreditState>> m_branchAccounts;
    std::unordered_map<std::uint32_t, std::weak_ptr<MediaGraphPayloadCreditState>> m_producerAccounts;
};

} // namespace media::ffmpeg::graph
