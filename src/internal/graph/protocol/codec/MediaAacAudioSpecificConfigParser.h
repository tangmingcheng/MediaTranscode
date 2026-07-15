#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

struct MediaParsedAacAudioSpecificConfig final {
    std::uint8_t audioObjectType;
    std::uint8_t samplingFrequencyIndex;
    std::uint8_t channelConfiguration;
    int sampleRate;
    int channels;
    int frameSamples;
};

inline ::media::Result<MediaParsedAacAudioSpecificConfig>
parseMediaAacAudioSpecificConfig(std::span<const std::uint8_t> bytes)
{
    constexpr std::uint8_t AacLcAudioObjectType = 2;
    constexpr int AacLongFrameSamples = 1024;
    constexpr int AacShortFrameSamples = 960;
    constexpr std::array<int, 13> SampleRates{
        96000, 88200, 64000, 48000, 44100, 32000, 24000,
        22050, 16000, 12000, 11025, 8000, 7350};

    if (bytes.size() != 2) {
        return ::media::Result<MediaParsedAacAudioSpecificConfig>::failure(
            ::media::ErrorInfo::unsupported(
                "AAC AudioSpecificConfig must use the exact supported two-byte form"));
    }
    const std::uint16_t bits = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
    const auto audioObjectType = static_cast<std::uint8_t>((bits >> 11) & 0x1F);
    const auto samplingFrequencyIndex =
        static_cast<std::uint8_t>((bits >> 7) & 0x0F);
    const auto channelConfiguration =
        static_cast<std::uint8_t>((bits >> 3) & 0x0F);
    const bool frameLengthFlag = ((bits >> 2) & 1) != 0;
    const bool dependsOnCoreCoder = ((bits >> 1) & 1) != 0;
    const bool extensionFlag = (bits & 1) != 0;
    const int channels = channelConfiguration <= 6
        ? channelConfiguration
        : channelConfiguration == 7 ? 8 : 0;
    if (audioObjectType != AacLcAudioObjectType ||
        samplingFrequencyIndex >= SampleRates.size() || channels == 0 ||
        dependsOnCoreCoder || extensionFlag) {
        return ::media::Result<MediaParsedAacAudioSpecificConfig>::failure(
            ::media::ErrorInfo::unsupported(
                "AAC requires explicit-frequency-free AAC-LC GASpecificConfig"));
    }
    return ::media::Result<MediaParsedAacAudioSpecificConfig>::success({
        audioObjectType,
        samplingFrequencyIndex,
        channelConfiguration,
        SampleRates[samplingFrequencyIndex],
        channels,
        frameLengthFlag ? AacShortFrameSamples : AacLongFrameSamples});
}

} // namespace media::ffmpeg::graph
