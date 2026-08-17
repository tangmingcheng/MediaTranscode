#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsMaterializedVideoConfig final {
public:
    static ::media::Result<MediaTsMaterializedVideoConfig> create(
        MediaTsNalLayout layout,
        std::uint8_t nalLengthBytes,
        std::vector<std::uint8_t> spsAnnexB,
        std::vector<std::uint8_t> ppsAnnexB);

    MediaTsNalLayout layout() const noexcept { return m_layout; }
    std::uint8_t nalLengthBytes() const noexcept { return m_nalLengthBytes; }
    const std::vector<std::uint8_t>& spsAnnexB() const noexcept { return m_spsAnnexB; }
    const std::vector<std::uint8_t>& ppsAnnexB() const noexcept { return m_ppsAnnexB; }

private:
    MediaTsMaterializedVideoConfig(
        MediaTsNalLayout layout,
        std::uint8_t nalLengthBytes,
        std::vector<std::uint8_t> spsAnnexB,
        std::vector<std::uint8_t> ppsAnnexB) noexcept;

    MediaTsNalLayout m_layout;
    std::uint8_t m_nalLengthBytes;
    std::vector<std::uint8_t> m_spsAnnexB;
    std::vector<std::uint8_t> m_ppsAnnexB;
};

class MediaTsMaterializedAudioConfig final {
public:
    static ::media::Result<MediaTsMaterializedAudioConfig> create(
        std::uint8_t audioObjectType,
        std::uint8_t samplingFrequencyIndex,
        std::uint8_t channelConfiguration);

    std::uint8_t audioObjectType() const noexcept { return m_audioObjectType; }
    std::uint8_t samplingFrequencyIndex() const noexcept
    {
        return m_samplingFrequencyIndex;
    }
    std::uint8_t channelConfiguration() const noexcept
    {
        return m_channelConfiguration;
    }

private:
    MediaTsMaterializedAudioConfig(
        std::uint8_t audioObjectType,
        std::uint8_t samplingFrequencyIndex,
        std::uint8_t channelConfiguration) noexcept;

    std::uint8_t m_audioObjectType;
    std::uint8_t m_samplingFrequencyIndex;
    std::uint8_t m_channelConfiguration;
};

} // namespace media::ffmpeg::graph
