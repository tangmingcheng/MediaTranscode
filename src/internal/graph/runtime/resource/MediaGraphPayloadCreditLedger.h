#pragma once

#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphPayloadCreditSnapshot final {
    std::uint64_t currentBytes = 0;
    std::uint64_t currentObjects = 0;
    std::uint64_t highWaterBytes = 0;
    std::uint64_t highWaterObjects = 0;
    std::uint64_t reservations = 0;
    std::uint64_t releases = 0;
    std::uint64_t pressureFailures = 0;
};

class MediaGraphPayloadCreditState;

class MediaGraphPayloadCreditReleaseObserver {
public:
    virtual ~MediaGraphPayloadCreditReleaseObserver() = default;
    virtual void onGraphPayloadCreditReleased() noexcept = 0;
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
    void setReleaseObserver(
        std::weak_ptr<MediaGraphPayloadCreditReleaseObserver> observer) noexcept;
    MediaGraphPayloadCreditSnapshot snapshot() const noexcept;
    const MediaGraphPayloadCreditPlan& plan() const noexcept { return m_plan; }

private:
    MediaGraphPayloadCreditLedger(
        MediaGraphPayloadCreditPlan plan,
        std::shared_ptr<MediaGraphPayloadCreditState> state) noexcept;

    MediaGraphPayloadCreditPlan m_plan;
    std::shared_ptr<MediaGraphPayloadCreditState> m_state;
};

} // namespace media::ffmpeg::graph
