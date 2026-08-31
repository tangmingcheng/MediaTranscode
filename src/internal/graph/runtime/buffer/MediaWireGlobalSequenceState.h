#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

class MediaNodeWakeup;

struct MediaWireGlobalSequenceSnapshot final {
    std::uint64_t generation;
    std::uint64_t nextGlobalSequence;
    bool reservationActive;
    bool poisoned;
    std::uint64_t currentDatagrams;
    std::uint64_t currentWireBytes;
    std::uint64_t highWaterDatagrams;
    std::uint64_t highWaterWireBytes;
    std::uint64_t maximumResidenceNanoseconds;
    std::optional<std::uint64_t> lastMaterializedSequence;
    std::optional<std::uint64_t> lastScheduledSequence;
    std::optional<std::uint64_t> lastSubmittedSequence;
    std::optional<std::uint64_t> lastCommittedSequence;
};

struct MediaWireGlobalSequenceReservationEntry final {
    std::uint64_t endpointId;
    std::uint64_t payloadBytes;
    MediaRunningTime materializedAt;
};

class MediaWireGlobalSequenceState;

class MediaWireGlobalSequenceReservation final {
public:
    MediaWireGlobalSequenceReservation(
        MediaWireGlobalSequenceReservation&& other) noexcept;
    MediaWireGlobalSequenceReservation& operator=(
        MediaWireGlobalSequenceReservation&& other) noexcept;
    MediaWireGlobalSequenceReservation(
        const MediaWireGlobalSequenceReservation&) = delete;
    MediaWireGlobalSequenceReservation& operator=(
        const MediaWireGlobalSequenceReservation&) = delete;
    ~MediaWireGlobalSequenceReservation() noexcept;

    std::uint64_t generation() const noexcept;
    std::size_t size() const noexcept;
    ::media::Result<std::uint64_t> sequence(std::size_t index) const noexcept;
    ::media::Status markScheduled(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept;
    ::media::Status markSubmitted(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept;
    ::media::Status canCommit(
        std::size_t begin, std::size_t count) const noexcept;
    ::media::Status commit(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept;

private:
    friend class MediaWireGlobalSequenceState;

    MediaWireGlobalSequenceReservation(
        std::shared_ptr<MediaWireGlobalSequenceState> state,
        std::uint64_t reservationIdentity,
        std::uint64_t firstSequence,
        std::vector<std::uint64_t> wireBytes,
        std::vector<MediaRunningTime> materializedAt) noexcept;
    void releaseCompleted() noexcept;
    void abandon() noexcept;

    std::shared_ptr<MediaWireGlobalSequenceState> m_state;
    std::uint64_t m_reservationIdentity = 0;
    std::uint64_t m_firstSequence = 0;
    std::vector<std::uint64_t> m_wireBytes;
    std::vector<MediaRunningTime> m_materializedAt;
    std::size_t m_committed = 0;
};

class MediaWireGlobalSequenceState final
    : public std::enable_shared_from_this<MediaWireGlobalSequenceState> {
public:
    static ::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>
    create(std::string sessionKey,
           std::string serviceScopeId,
           std::uint64_t generation,
           std::uint64_t firstGlobalSequence,
           std::size_t maximumOutstandingDatagrams,
           std::uint64_t maximumOutstandingWireBytes,
           std::unordered_map<std::uint64_t, std::uint64_t>
               endpointWireHeaderBytes);

    ::media::Result<MediaWireGlobalSequenceReservation> reserve(
        std::span<const MediaWireGlobalSequenceReservationEntry> entries);
    ::media::Status registerReservationWakeup(
        std::uint64_t endpointId,
        std::shared_ptr<MediaNodeWakeup> wakeup);
    MediaWireGlobalSequenceSnapshot snapshot() const noexcept;
    const std::string& sessionKey() const noexcept { return m_sessionKey; }
    const std::string& serviceScopeId() const noexcept
    {
        return m_serviceScopeId;
    }
    std::uint64_t generation() const noexcept { return m_generation; }

private:
    friend class MediaWireGlobalSequenceReservation;

    MediaWireGlobalSequenceState(std::string sessionKey,
                                 std::string serviceScopeId,
                                 std::uint64_t generation,
                                 std::uint64_t firstGlobalSequence,
                                 std::size_t maximumOutstandingDatagrams,
                                 std::uint64_t maximumOutstandingWireBytes,
                                 std::unordered_map<std::uint64_t, std::uint64_t>
                                     endpointWireHeaderBytes) noexcept;
    ::media::Status markStageRange(
        const MediaWireGlobalSequenceReservation& reservation,
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now,
        std::optional<std::uint64_t>& lastStageSequence) noexcept;
    ::media::Status markStageRangeLocked(
        const MediaWireGlobalSequenceReservation& reservation,
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now,
        std::optional<std::uint64_t>& lastStageSequence) noexcept;
    ::media::Status canCommitRangeLocked(
        const MediaWireGlobalSequenceReservation& reservation,
        std::size_t begin,
        std::size_t count) const noexcept;
    void observeResidence(
        MediaRunningTime materializedAt, MediaRunningTime now) noexcept;
    void notifyReservationWaiters() noexcept;

    struct ReservationRecord final {
        std::uint64_t identity;
        std::uint64_t firstSequence;
        std::size_t count;
        std::size_t committed;
    };

    mutable std::mutex m_mutex;
    const std::string m_sessionKey;
    const std::string m_serviceScopeId;
    const std::uint64_t m_generation;
    const std::size_t m_maximumOutstandingDatagrams;
    const std::uint64_t m_maximumOutstandingWireBytes;
    const std::unordered_map<std::uint64_t, std::uint64_t>
        m_endpointWireHeaderBytes;
    std::unordered_map<std::uint64_t, std::weak_ptr<MediaNodeWakeup>>
        m_reservationWakeups;
    std::uint64_t m_nextGlobalSequence;
    std::uint64_t m_projectedNextGlobalSequence;
    std::uint64_t m_nextReservationIdentity = 1;
    std::size_t m_outstandingDatagrams = 0;
    std::uint64_t m_outstandingWireBytes = 0;
    std::uint64_t m_highWaterDatagrams = 0;
    std::uint64_t m_highWaterWireBytes = 0;
    std::uint64_t m_maximumResidenceNanoseconds = 0;
    std::optional<std::uint64_t> m_lastMaterializedSequence;
    std::optional<std::uint64_t> m_lastScheduledSequence;
    std::optional<std::uint64_t> m_lastSubmittedSequence;
    std::optional<std::uint64_t> m_lastCommittedSequence;
    std::deque<ReservationRecord> m_reservations;
    bool m_reservationBlocked = false;
    bool m_poisoned = false;
};

} // namespace media::ffmpeg::graph
