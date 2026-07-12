#include "internal/graph/protocol/rtp/MediaAacAudioSpecificConfig.h"

#include <iterator>

namespace media::ffmpeg::graph {
namespace {

constexpr int AacLcAudioObjectType = 2;
constexpr int AacLongFrameSamples = 1024;
constexpr int AacShortFrameSamples = 960;
constexpr int AacSampleRates[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000,
    22050, 16000, 12000, 11025, 8000, 7350
};

int channelCount(int configuration) noexcept
{
    if (configuration >= 1 && configuration <= 6) return configuration;
    if (configuration == 7) return 8;
    return 0;
}

} // namespace

::media::Result<MediaAacAudioSpecificConfig> parseAacAudioSpecificConfig(
    const std::vector<uint8_t>& bytes)
{
    if (bytes.size() != 2) return ::media::Result<MediaAacAudioSpecificConfig>::failure(
        ::media::ErrorInfo::unsupported("Raw RTP AAC AudioSpecificConfig must use the exact supported two-byte form"));

    const uint16_t bits = static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
    const int audioObjectType = (bits >> 11) & 0x1f;
    const int samplingFrequencyIndex = (bits >> 7) & 0x0f;
    const int channelConfiguration = (bits >> 3) & 0x0f;
    const bool frameLengthFlag = ((bits >> 2) & 0x01) != 0;
    const bool dependsOnCoreCoder = ((bits >> 1) & 0x01) != 0;
    const bool extensionFlag = (bits & 0x01) != 0;
    if (audioObjectType != AacLcAudioObjectType ||
        samplingFrequencyIndex >= static_cast<int>(std::size(AacSampleRates)) ||
        channelCount(channelConfiguration) == 0 || dependsOnCoreCoder || extensionFlag) {
        return ::media::Result<MediaAacAudioSpecificConfig>::failure(
            ::media::ErrorInfo::unsupported(
                "Raw RTP AAC supports explicit-frequency-free AAC-LC GASpecificConfig only"));
    }

    return ::media::Result<MediaAacAudioSpecificConfig>::success({
        AacSampleRates[samplingFrequencyIndex],
        channelCount(channelConfiguration),
        frameLengthFlag ? AacShortFrameSamples : AacLongFrameSamples
    });
}

} // namespace media::ffmpeg::graph
