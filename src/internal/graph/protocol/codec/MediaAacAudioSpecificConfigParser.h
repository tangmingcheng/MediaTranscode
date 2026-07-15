#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

inline constexpr std::uint8_t MediaAacLcAudioObjectType = 2;
inline constexpr int MediaAacLongFrameSamples = 1024;
inline constexpr int MediaAacShortFrameSamples = 960;
inline constexpr std::array<int, 13> MediaAacSampleRates{
    96000, 88200, 64000, 48000, 44100, 32000, 24000,
    22050, 16000, 12000, 11025, 8000, 7350};

struct MediaParsedAacAudioSpecificConfig final {
    std::uint8_t audioObjectType;
    std::uint8_t samplingFrequencyIndex;
    std::uint8_t channelConfiguration;
    int sampleRate;
    int channels;
    int frameSamples;
};

inline ::media::Result<std::array<std::uint8_t, 2>>
makeMediaAacLcLongFrameAudioSpecificConfig(int sampleRate, int channels)
{
    std::size_t samplingFrequencyIndex = MediaAacSampleRates.size();
    for (std::size_t index = 0; index < MediaAacSampleRates.size(); ++index) {
        if (MediaAacSampleRates[index] == sampleRate) {
            samplingFrequencyIndex = index;
            break;
        }
    }
    const int channelConfiguration =
        channels >= 1 && channels <= 6 ? channels : channels == 8 ? 7 : 0;
    if (samplingFrequencyIndex == MediaAacSampleRates.size() ||
        channelConfiguration == 0) {
        return ::media::Result<std::array<std::uint8_t, 2>>::failure(
            ::media::ErrorInfo::unsupported(
                "AAC-LC long-frame configuration is not representable"));
    }
    const std::uint16_t bits = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(MediaAacLcAudioObjectType) << 11) |
        (static_cast<std::uint16_t>(samplingFrequencyIndex) << 7) |
        (static_cast<std::uint16_t>(channelConfiguration) << 3));
    return ::media::Result<std::array<std::uint8_t, 2>>::success({
        static_cast<std::uint8_t>(bits >> 8),
        static_cast<std::uint8_t>(bits & 0xFF)});
}

inline ::media::Result<MediaParsedAacAudioSpecificConfig>
parseMediaAacAudioSpecificConfig(std::span<const std::uint8_t> bytes)
{
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
    if (audioObjectType != MediaAacLcAudioObjectType ||
        samplingFrequencyIndex >= MediaAacSampleRates.size() || channels == 0 ||
        dependsOnCoreCoder || extensionFlag) {
        return ::media::Result<MediaParsedAacAudioSpecificConfig>::failure(
            ::media::ErrorInfo::unsupported(
                "AAC requires explicit-frequency-free AAC-LC GASpecificConfig"));
    }
    return ::media::Result<MediaParsedAacAudioSpecificConfig>::success({
        audioObjectType,
        samplingFrequencyIndex,
        channelConfiguration,
        MediaAacSampleRates[samplingFrequencyIndex],
        channels,
        frameLengthFlag ? MediaAacShortFrameSamples : MediaAacLongFrameSamples});
}

} // namespace media::ffmpeg::graph
