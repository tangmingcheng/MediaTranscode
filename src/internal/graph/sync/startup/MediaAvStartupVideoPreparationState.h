#pragma once

#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/runtime/channel/MediaReservedOutputTransaction.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

class MediaNodeWakeup;

enum class MediaAvStartupVideoPreparationPhase {
    Awaiting,
    Feeding,
    FilterReady,
    ReleaseCommitted,
    Cancelled
};

struct MediaAvStartupVideoPreparationSnapshot final {
    MediaAvSyncGroupKey groupKey;
    MediaAvStartupVideoPreparationPhase phase =
        MediaAvStartupVideoPreparationPhase::Awaiting;
    std::uint64_t generation = 0;
    std::uint64_t releaseIdentity = 0;
    std::size_t committedVideoUnits = 0;
    std::size_t videoUnitCount = 0;
    bool filterOutputReserved = false;
    bool extractorOutputsReserved = false;
    std::optional<MediaPlaybackEpoch> anchoredEpoch;
    std::optional<MediaAudioPlaybackOrigin> anchoredAudioOrigin;
    bool extractorOutputsReanchored = false;
};

enum class MediaAvStartupVideoReservationKind {
    NoReservation,
    Reserved
};

struct MediaAvStartupVideoReservation final {
    MediaAvStartupVideoReservationKind kind =
        MediaAvStartupVideoReservationKind::NoReservation;
    std::optional<std::size_t> index;
};

class MediaAvStartupVideoPreparationState final {
public:
    using VideoReservation = ::media::Result<MediaAvStartupVideoReservation>;

    static ::media::Result<std::shared_ptr<MediaAvStartupVideoPreparationState>>
    create(MediaAvSyncGroupKey groupKey);

    ::media::Status begin(std::uint64_t generation,
                          std::uint64_t releaseIdentity,
                          std::size_t videoUnitCount);
    VideoReservation reserveNextVideoUnit(std::uint64_t generation,
                                          std::uint64_t releaseIdentity);
    ::media::Status commitVideoUnit(std::uint64_t generation,
                                    std::uint64_t releaseIdentity,
                                    std::size_t index);
    ::media::Status markFilterReady(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        MediaOutputCapacityReservationHandle reservation);
    ::media::Status registerExtractorOutputs(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        MediaOutputCapacityReservationHandle reservation);
    ::media::Status publishInitialAnchor(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);
    ::media::Status acknowledgeExtractorReanchor(
        std::uint64_t generation,
        std::uint64_t releaseIdentity);
    ::media::Status authorizeRelease(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        const MediaReservedOutputTransaction::Authorization& activation);
    ::media::Status cancel();

    ::media::Status bindSequencerWakeup(
        const std::shared_ptr<MediaNodeWakeup>& wakeup);
    ::media::Status bindFilterWakeup(
        const std::shared_ptr<MediaNodeWakeup>& wakeup);
    ::media::Status bindExtractorWakeup(
        const std::shared_ptr<MediaNodeWakeup>& wakeup);
    MediaAvStartupVideoPreparationSnapshot snapshot() const;

private:
    explicit MediaAvStartupVideoPreparationState(MediaAvSyncGroupKey groupKey);
    ::media::Status validateIdentityLocked(
        std::uint64_t generation,
        std::uint64_t releaseIdentity) const;
    ::media::Status failureLocked(const char* operation) const;

    mutable std::mutex m_mutex;
    MediaAvSyncGroupKey m_groupKey;
    MediaAvStartupVideoPreparationPhase m_phase =
        MediaAvStartupVideoPreparationPhase::Awaiting;
    std::uint64_t m_generation = 0;
    std::uint64_t m_releaseIdentity = 0;
    std::size_t m_committedVideoUnits = 0;
    std::size_t m_videoUnitCount = 0;
    std::optional<std::size_t> m_reservedVideoUnit;
    std::optional<::media::ErrorInfo> m_terminalError;
    std::weak_ptr<MediaNodeWakeup> m_sequencerWakeup;
    std::weak_ptr<MediaNodeWakeup> m_filterWakeup;
    std::weak_ptr<MediaNodeWakeup> m_extractorWakeup;
    MediaOutputCapacityReservationHandle m_filterOutputReservation;
    MediaOutputCapacityReservationHandle m_extractorOutputsReservation;
    std::optional<MediaPlaybackEpoch> m_anchoredEpoch;
    std::optional<MediaAudioPlaybackOrigin> m_anchoredAudioOrigin;
    bool m_extractorOutputsReanchored = false;
};

} // namespace media::ffmpeg::graph
