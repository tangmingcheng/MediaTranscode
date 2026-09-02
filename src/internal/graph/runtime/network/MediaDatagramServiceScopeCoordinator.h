#pragma once

#include "internal/graph/runtime/network/MediaDatagramPacingController.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <stop_token>

namespace media::ffmpeg::graph {

struct MediaDatagramServiceScopeState;

struct MediaDatagramServiceScopeTelemetry final {
    std::uint64_t activeMembers = 0;
    std::uint64_t highWaterMembers = 0;
    std::uint64_t admittedWireBytesPerSecond = 0;
    std::uint64_t maximumWireBytesPerSecond = 0;
    std::uint64_t reservedDatagrams = 0;
    std::uint64_t submittedDatagrams = 0;
    std::uint64_t cancelledReservations = 0;
    std::uint64_t contentionWaits = 0;
    std::uint64_t deadlineRejections = 0;
    std::uint64_t ambiguousSubmissions = 0;
    bool counterSaturated = false;
};

class MediaDatagramServiceScopeReservation final {
public:
    ~MediaDatagramServiceScopeReservation() noexcept;
    MediaDatagramServiceScopeReservation(
        MediaDatagramServiceScopeReservation&& other) noexcept;
    MediaDatagramServiceScopeReservation& operator=(
        MediaDatagramServiceScopeReservation&& other) noexcept;
    MediaDatagramServiceScopeReservation(
        const MediaDatagramServiceScopeReservation&) = delete;
    MediaDatagramServiceScopeReservation& operator=(
        const MediaDatagramServiceScopeReservation&) = delete;

    std::chrono::steady_clock::time_point notBefore() const noexcept
    {
        return m_notBefore;
    }
    ::media::Status markSubmitted(
        std::chrono::steady_clock::time_point submitStartedAt,
        std::chrono::steady_clock::time_point submitCompletedAt);
    ::media::Status markAmbiguous(::media::ErrorInfo cause);

private:
    friend class MediaDatagramServiceScopeMembership;
    MediaDatagramServiceScopeReservation(
        std::shared_ptr<MediaDatagramServiceScopeState> state,
        std::uint64_t memberId,
        std::uint64_t reservationId,
        std::chrono::steady_clock::time_point notBefore,
        std::chrono::steady_clock::time_point notAfter,
        std::chrono::nanoseconds serviceDuration) noexcept;
    void cancel() noexcept;

    std::shared_ptr<MediaDatagramServiceScopeState> m_state;
    std::uint64_t m_memberId = 0;
    std::uint64_t m_reservationId = 0;
    std::chrono::steady_clock::time_point m_notBefore{};
    std::chrono::steady_clock::time_point m_notAfter{};
    std::chrono::nanoseconds m_serviceDuration{0};
};

class MediaDatagramServiceScopeMembership final {
public:
    ~MediaDatagramServiceScopeMembership() noexcept;
    MediaDatagramServiceScopeMembership(
        const MediaDatagramServiceScopeMembership&) = delete;
    MediaDatagramServiceScopeMembership& operator=(
        const MediaDatagramServiceScopeMembership&) = delete;

    static ::media::Result<
        std::unique_ptr<MediaDatagramServiceScopeMembership>>
    join(MediaDatagramPacingContract contract);

    ::media::Status rebind(MediaDatagramPacingContract contract);
    ::media::Result<MediaDatagramServiceScopeReservation> reserve(
        std::uint64_t wireBytes,
        std::chrono::steady_clock::time_point immutableSubmitDeadline,
        std::stop_token stopToken);
    MediaDatagramServiceScopeTelemetry telemetry() const noexcept;

private:
    MediaDatagramServiceScopeMembership(
        std::shared_ptr<MediaDatagramServiceScopeState> state,
        std::uint64_t memberId) noexcept;

    std::shared_ptr<MediaDatagramServiceScopeState> m_state;
    std::uint64_t m_memberId;
};

} // namespace media::ffmpeg::graph
