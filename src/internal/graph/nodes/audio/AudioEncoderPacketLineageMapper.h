#pragma once

#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAudioLineageCapacity;

class AudioEncoderPacketLineageMapper final {
public:
    // Single-use transaction. The caller holds the lineage-state lock and keeps
    // this mapper alive and unchanged until commit or destruction. After EAGAIN,
    // destroy it before receive/map and prepare a new transaction for retry.
    class PreparedSubmission final {
    public:
        PreparedSubmission(PreparedSubmission&&) noexcept = default;
        PreparedSubmission& operator=(PreparedSubmission&&) noexcept = default;
        void commit() && noexcept;

    private:
        friend class AudioEncoderPacketLineageMapper;
        PreparedSubmission(AudioEncoderPacketLineageMapper& owner,
                           std::unique_ptr<MediaAudioIntervalAccumulator> intervals,
                           std::int64_t framePts, int frameSamples) noexcept;
        AudioEncoderPacketLineageMapper* m_owner;
        std::unique_ptr<MediaAudioIntervalAccumulator> m_intervals;
        std::int64_t m_framePts;
        int m_frameSamples;
    };

    ::media::Result<PreparedSubmission> prepareSubmission(
        std::int64_t framePts,
        int frameSamples,
        const std::vector<MediaAudioIntervalFragment>& fragments);
    ::media::Result<std::optional<std::vector<MediaAudioIntervalFragment>>> map(
        std::int64_t packetPts,
        std::int64_t packetDuration);

    ::media::Status observeLineageCapacity(
        MediaAudioLineageCapacity& capacity) const;
    ::media::Status finish() const;
    void reset() noexcept;

private:
    MediaAudioIntervalAccumulator m_intervals;
    std::optional<std::int64_t> m_nextSubmittedPts;
    std::optional<std::int64_t> m_nextPacketPts;
    std::optional<std::int64_t> m_nextPrimingPts;
};

} // namespace media::ffmpeg::graph
