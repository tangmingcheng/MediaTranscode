#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsMaterializedVideoConfig final {
public:
    static ::media::Result<MediaTsMaterializedVideoConfig> create(
        MediaTsVideoElementaryStreamContract contract,
        std::vector<std::vector<std::uint8_t>> parameterSetsAnnexB);

    const MediaTsVideoElementaryStreamContract& contract() const noexcept
    {
        return m_contract;
    }
    const std::vector<std::vector<std::uint8_t>>&
    parameterSetsAnnexB() const noexcept
    {
        return m_parameterSetsAnnexB;
    }

private:
    MediaTsMaterializedVideoConfig(
        MediaTsVideoElementaryStreamContract contract,
        std::vector<std::vector<std::uint8_t>> parameterSetsAnnexB) noexcept;

    MediaTsVideoElementaryStreamContract m_contract;
    std::vector<std::vector<std::uint8_t>> m_parameterSetsAnnexB;
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
