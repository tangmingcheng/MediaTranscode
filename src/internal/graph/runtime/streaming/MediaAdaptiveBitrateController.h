#pragma once

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaBitrateLadderEntry {
    int width = 0;
    int height = 0;
    int64_t bitrate = 0;
    int frameRate = 0;
};

struct MediaAdaptiveBitrateSample {
    int64_t throughputBitsPerSecond = 0;
    int64_t queueDelayUs = 0;
    double packetLossRate = 0.0;
};

class MediaAdaptiveBitrateController final {
public:
    void setLadder(std::vector<MediaBitrateLadderEntry> ladder);
    void reset();

    const MediaBitrateLadderEntry* current() const noexcept;
    const MediaBitrateLadderEntry* update(const MediaAdaptiveBitrateSample& sample);

private:
    bool shouldDowngrade(const MediaAdaptiveBitrateSample& sample) const noexcept;
    bool shouldUpgrade(const MediaAdaptiveBitrateSample& sample) const noexcept;

private:
    std::vector<MediaBitrateLadderEntry> m_ladder;
    std::size_t m_currentIndex = 0;
    uint32_t m_stableSamples = 0;
};

} // namespace media::ffmpeg::graph
