#include "internal/graph/runtime/streaming/MediaAdaptiveBitrateController.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

void MediaAdaptiveBitrateController::setLadder(std::vector<MediaBitrateLadderEntry> ladder)
{
    std::sort(ladder.begin(), ladder.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.bitrate < rhs.bitrate;
    });

    m_ladder = std::move(ladder);
    m_currentIndex = m_ladder.empty() ? 0 : m_ladder.size() - 1;
    m_stableSamples = 0;
}

void MediaAdaptiveBitrateController::reset()
{
    m_currentIndex = m_ladder.empty() ? 0 : m_ladder.size() - 1;
    m_stableSamples = 0;
}

const MediaBitrateLadderEntry* MediaAdaptiveBitrateController::current() const noexcept
{
    if (m_ladder.empty() || m_currentIndex >= m_ladder.size()) {
        return nullptr;
    }

    return &m_ladder[m_currentIndex];
}

const MediaBitrateLadderEntry* MediaAdaptiveBitrateController::update(const MediaAdaptiveBitrateSample& sample)
{
    if (m_ladder.empty()) {
        return nullptr;
    }

    if (shouldDowngrade(sample) && m_currentIndex > 0) {
        --m_currentIndex;
        m_stableSamples = 0;
        return current();
    }

    if (shouldUpgrade(sample) && m_currentIndex + 1 < m_ladder.size()) {
        ++m_currentIndex;
        m_stableSamples = 0;
        return current();
    }

    ++m_stableSamples;
    return current();
}

bool MediaAdaptiveBitrateController::shouldDowngrade(const MediaAdaptiveBitrateSample& sample) const noexcept
{
    const MediaBitrateLadderEntry* active = current();
    if (!active) {
        return false;
    }

    if (sample.packetLossRate >= 0.05) {
        return true;
    }

    if (sample.queueDelayUs >= 200000) {
        return true;
    }

    return sample.throughputBitsPerSecond > 0 &&
           sample.throughputBitsPerSecond < active->bitrate * 12 / 10;
}

bool MediaAdaptiveBitrateController::shouldUpgrade(const MediaAdaptiveBitrateSample& sample) const noexcept
{
    const MediaBitrateLadderEntry* active = current();
    if (!active || m_stableSamples < 8) {
        return false;
    }

    return sample.packetLossRate < 0.01 &&
           sample.queueDelayUs < 50000 &&
           sample.throughputBitsPerSecond > active->bitrate * 2;
}

} // namespace media::ffmpeg::graph
