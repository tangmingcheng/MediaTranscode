#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"

#include <algorithm>

namespace media::ffmpeg::graph {

MediaTsEvidenceTimeline::MediaTsEvidenceTimeline(
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes) noexcept
    : m_capacity(capacity)
    , m_maximumPositionRegressionBytes(maximumPositionRegressionBytes)
{
}

::media::Result<MediaTsEvidenceTimeline> MediaTsEvidenceTimeline::create(
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes)
{
    if (capacity == 0) {
        return ::media::Result<MediaTsEvidenceTimeline>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS evidence timeline capacity must be positive"));
    }
    return ::media::Result<MediaTsEvidenceTimeline>::success(
        MediaTsEvidenceTimeline(capacity, maximumPositionRegressionBytes));
}

::media::Status MediaTsEvidenceTimeline::append(MediaTsEvidenceCheckpoint checkpoint)
{
    if (checkpoint.pcrObservation &&
        checkpoint.pcrObservation->byteOffset != checkpoint.byteOffset) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR evidence offset must match its checkpoint"));
    }
    if (checkpoint.continuityEvent &&
        checkpoint.continuityEvent->byteOffset != checkpoint.byteOffset) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS continuity event offset must match its checkpoint"));
    }
    if (!m_checkpoints.empty() && checkpoint.byteOffset <= m_checkpoints.back().byteOffset) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS evidence offsets must increase strictly"));
    }
    evictSafeCheckpoints();
    if (m_checkpoints.size() == m_capacity) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS evidence capacity exhausted before safe eviction"));
    }
    m_checkpoints.push_back(std::move(checkpoint));
    return ::media::Status::success();
}

::media::Result<MediaTsEvidenceCheckpoint> MediaTsEvidenceTimeline::atOrBefore(
    std::uint64_t packetPosition) const
{
    if (m_highWatermark && *m_highWatermark > packetPosition &&
        *m_highWatermark - packetPosition > m_maximumPositionRegressionBytes) {
        return ::media::Result<MediaTsEvidenceCheckpoint>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS packet position exceeds planned regression"));
    }
    const auto it = std::upper_bound(
        m_checkpoints.begin(), m_checkpoints.end(), packetPosition,
        [](std::uint64_t position, const MediaTsEvidenceCheckpoint& item) {
            return position < item.byteOffset;
        });
    if (it == m_checkpoints.begin()) {
        return ::media::Result<MediaTsEvidenceCheckpoint>::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS evidence is unavailable for packet position"));
    }
    return ::media::Result<MediaTsEvidenceCheckpoint>::success(*std::prev(it));
}

::media::Status MediaTsEvidenceTimeline::observePacketPosition(std::uint64_t packetPosition)
{
    if (m_highWatermark && *m_highWatermark > packetPosition &&
        *m_highWatermark - packetPosition > m_maximumPositionRegressionBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS packet position exceeds planned regression"));
    }
    if (!m_highWatermark || packetPosition > *m_highWatermark) {
        m_highWatermark = packetPosition;
        evictSafeCheckpoints();
    }
    return ::media::Status::success();
}

::media::Result<std::vector<MediaTsEvidenceCheckpoint>>
MediaTsEvidenceTimeline::snapshotAfter(
    std::optional<std::uint64_t> exclusiveOffset) const
{
    const auto first = exclusiveOffset
        ? std::upper_bound(
              m_checkpoints.begin(), m_checkpoints.end(), *exclusiveOffset,
              [](std::uint64_t offset, const MediaTsEvidenceCheckpoint& item) {
                  return offset < item.byteOffset;
              })
        : m_checkpoints.begin();
    return ::media::Result<std::vector<MediaTsEvidenceCheckpoint>>::success(
        std::vector<MediaTsEvidenceCheckpoint>(first, m_checkpoints.end()));
}

::media::Result<std::vector<std::uint64_t>>
MediaTsEvidenceTimeline::completeContinuityOffsetsFor(
    std::span<const std::uint16_t> sourcePids) const
{
    if (sourcePids.empty()) {
        return ::media::Result<std::vector<std::uint64_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS source PID set must not be empty"));
    }
    if (!m_historyComplete) {
        return ::media::Result<std::vector<std::uint64_t>>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS continuity history was evicted before runtime binding"));
    }
    std::vector<std::uint64_t> offsets;
    for (const auto& checkpoint : m_checkpoints) {
        if (!checkpoint.continuityEvent) continue;
        const auto pid = checkpoint.continuityEvent->pid;
        if (std::find(sourcePids.begin(), sourcePids.end(), pid) != sourcePids.end()) {
            offsets.push_back(checkpoint.byteOffset);
        }
    }
    return ::media::Result<std::vector<std::uint64_t>>::success(std::move(offsets));
}

void MediaTsEvidenceTimeline::evictSafeCheckpoints()
{
    if (!m_highWatermark || *m_highWatermark <= m_maximumPositionRegressionBytes) return;
    const std::uint64_t earliestLegalPosition =
        *m_highWatermark - m_maximumPositionRegressionBytes;
    while (m_checkpoints.size() > 1 &&
           m_checkpoints[1].byteOffset <= earliestLegalPosition) {
        m_checkpoints.pop_front();
        m_historyComplete = false;
    }
}

} // namespace media::ffmpeg::graph
