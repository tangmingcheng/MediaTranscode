#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace media::ffmpeg::graph {

struct MediaWireGlobalSequenceSnapshot final {
    std::uint64_t generation;
    std::uint64_t nextGlobalSequence;
    bool reservationActive;
    bool poisoned;
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
    ::media::Status canCommit(std::size_t index) const noexcept;
    ::media::Status commit(std::size_t index) noexcept;

private:
    friend class MediaWireGlobalSequenceState;

    MediaWireGlobalSequenceReservation(
        std::shared_ptr<MediaWireGlobalSequenceState> state,
        std::size_t count,
        std::unique_lock<std::mutex> lock) noexcept;
    void releaseCompleted() noexcept;
    void abandon() noexcept;

    std::shared_ptr<MediaWireGlobalSequenceState> m_state;
    std::uint64_t m_firstSequence = 0;
    std::size_t m_count = 0;
    std::size_t m_committed = 0;
    std::unique_lock<std::mutex> m_lock;
};

class MediaWireGlobalSequenceState final
    : public std::enable_shared_from_this<MediaWireGlobalSequenceState> {
public:
    static ::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>
    create(std::string sessionKey,
           std::uint64_t serviceScopeId,
           std::uint64_t generation,
           std::uint64_t firstGlobalSequence);

    ::media::Result<MediaWireGlobalSequenceReservation> reserve(
        std::size_t count);
    MediaWireGlobalSequenceSnapshot snapshot() const noexcept;
    const std::string& sessionKey() const noexcept { return m_sessionKey; }
    std::uint64_t serviceScopeId() const noexcept
    {
        return m_serviceScopeId;
    }
    std::uint64_t generation() const noexcept { return m_generation; }

private:
    friend class MediaWireGlobalSequenceReservation;

    MediaWireGlobalSequenceState(std::string sessionKey,
                                 std::uint64_t serviceScopeId,
                                 std::uint64_t generation,
                                 std::uint64_t firstGlobalSequence) noexcept;

    mutable std::mutex m_mutex;
    const std::string m_sessionKey;
    const std::uint64_t m_serviceScopeId;
    const std::uint64_t m_generation;
    std::uint64_t m_nextGlobalSequence;
    bool m_reservationActive = false;
    bool m_poisoned = false;
};

} // namespace media::ffmpeg::graph
