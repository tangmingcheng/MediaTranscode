#pragma once

#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAudioLineageCapacity;

class AudioEncoderPacketLineageMapper final {
public:
    ::media::Status submit(
        std::int64_t framePts,
        int frameSamples,
        std::vector<MediaAudioIntervalFragment> fragments);
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
