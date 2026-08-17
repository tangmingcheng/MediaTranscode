#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool canonicalParameterSet(const std::vector<std::uint8_t>& bytes,
                           std::uint8_t nalType) noexcept
{
    if (bytes.size() < 5 || bytes[0] != 0 || bytes[1] != 0 ||
        bytes[2] != 0 || bytes[3] != 1 || (bytes[4] & 0x80) != 0 ||
        (bytes[4] & 0x1F) != nalType) {
        return false;
    }
    for (std::size_t offset = 5; offset + 2 < bytes.size(); ++offset) {
        if (bytes[offset] == 0 && bytes[offset + 1] == 0 &&
            (bytes[offset + 2] == 1 ||
             (offset + 3 < bytes.size() && bytes[offset + 2] == 0 &&
              bytes[offset + 3] == 1))) {
            return false;
        }
    }
    return true;
}

} // namespace

::media::Result<MediaTsMaterializedVideoConfig>
MediaTsMaterializedVideoConfig::create(
    MediaTsNalLayout layout,
    std::uint8_t nalLengthBytes,
    std::vector<std::uint8_t> spsAnnexB,
    std::vector<std::uint8_t> ppsAnnexB)
{
    switch (layout) {
    case MediaTsNalLayout::AnnexB:
    case MediaTsNalLayout::LengthPrefixed:
        break;
    default:
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS materialized H.264 layout is invalid"));
    }
    if ((layout == MediaTsNalLayout::AnnexB && nalLengthBytes != 0) ||
        (layout == MediaTsNalLayout::LengthPrefixed &&
         (nalLengthBytes < 1 || nalLengthBytes > 4)) ||
        !canonicalParameterSet(spsAnnexB, 7) ||
        !canonicalParameterSet(ppsAnnexB, 8)) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS materialized H.264 config is invalid"));
    }
    return ::media::Result<MediaTsMaterializedVideoConfig>::success(
        MediaTsMaterializedVideoConfig(
            layout, nalLengthBytes, std::move(spsAnnexB), std::move(ppsAnnexB)));
}

MediaTsMaterializedVideoConfig::MediaTsMaterializedVideoConfig(
    MediaTsNalLayout layout,
    std::uint8_t nalLengthBytes,
    std::vector<std::uint8_t> spsAnnexB,
    std::vector<std::uint8_t> ppsAnnexB) noexcept
    : m_layout(layout)
    , m_nalLengthBytes(nalLengthBytes)
    , m_spsAnnexB(std::move(spsAnnexB))
    , m_ppsAnnexB(std::move(ppsAnnexB))
{
}

::media::Result<MediaTsMaterializedAudioConfig>
MediaTsMaterializedAudioConfig::create(
    std::uint8_t audioObjectType,
    std::uint8_t samplingFrequencyIndex,
    std::uint8_t channelConfiguration)
{
    if (audioObjectType < 1 || audioObjectType > 4 ||
        samplingFrequencyIndex > 12 ||
        channelConfiguration < 1 || channelConfiguration > 7) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS materialized AAC config is invalid"));
    }
    return ::media::Result<MediaTsMaterializedAudioConfig>::success(
        MediaTsMaterializedAudioConfig(
            audioObjectType, samplingFrequencyIndex, channelConfiguration));
}

MediaTsMaterializedAudioConfig::MediaTsMaterializedAudioConfig(
    std::uint8_t audioObjectType,
    std::uint8_t samplingFrequencyIndex,
    std::uint8_t channelConfiguration) noexcept
    : m_audioObjectType(audioObjectType)
    , m_samplingFrequencyIndex(samplingFrequencyIndex)
    , m_channelConfiguration(channelConfiguration)
{
}

} // namespace media::ffmpeg::graph
