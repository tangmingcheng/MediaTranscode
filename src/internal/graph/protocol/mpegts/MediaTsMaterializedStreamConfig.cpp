#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"

#include <array>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::optional<std::uint8_t> parameterSetType(
    MediaTsVideoCodec codec,
    const std::vector<std::uint8_t>& bytes) noexcept
{
    const std::size_t minimumSize = codec == MediaTsVideoCodec::H264 ? 5 : 6;
    if (bytes.size() < minimumSize || bytes[0] != 0 || bytes[1] != 0 ||
        bytes[2] != 0 || bytes[3] != 1 || (bytes[4] & 0x80) != 0) {
        return std::nullopt;
    }
    if (codec == MediaTsVideoCodec::Hevc && (bytes[5] & 0x07) == 0) {
        return std::nullopt;
    }
    for (std::size_t offset = minimumSize;
         offset + 2 < bytes.size(); ++offset) {
        if (bytes[offset] == 0 && bytes[offset + 1] == 0 &&
            (bytes[offset + 2] == 1 ||
             (offset + 3 < bytes.size() && bytes[offset + 2] == 0 &&
              bytes[offset + 3] == 1))) {
            return std::nullopt;
        }
    }
    return codec == MediaTsVideoCodec::H264
        ? std::optional<std::uint8_t>(bytes[4] & 0x1F)
        : std::optional<std::uint8_t>((bytes[4] >> 1) & 0x3F);
}

bool expectedParameterSetTypes(
    MediaTsVideoCodec codec,
    const std::vector<std::vector<std::uint8_t>>& parameterSets) noexcept
{
    const std::array<std::uint8_t, 3> expected =
        codec == MediaTsVideoCodec::H264
            ? std::array<std::uint8_t, 3>{7, 8, 0}
            : std::array<std::uint8_t, 3>{32, 33, 34};
    const std::size_t expectedCount =
        codec == MediaTsVideoCodec::H264 ? 2 : 3;
    if (parameterSets.size() != expectedCount) return false;
    for (std::size_t index = 0; index < expectedCount; ++index) {
        const auto type = parameterSetType(codec, parameterSets[index]);
        if (!type || *type != expected[index]) return false;
    }
    return true;
}

} // namespace

::media::Result<MediaTsMaterializedVideoConfig>
MediaTsMaterializedVideoConfig::create(
    MediaTsVideoElementaryStreamContract contract,
    std::vector<std::vector<std::uint8_t>> parameterSetsAnnexB)
{
    if (!expectedParameterSetTypes(
            contract.codec(), parameterSetsAnnexB)) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS materialized video parameter sets are incomplete, duplicated, out of order, or malformed"));
    }
    return ::media::Result<MediaTsMaterializedVideoConfig>::success(
        MediaTsMaterializedVideoConfig(
            std::move(contract), std::move(parameterSetsAnnexB)));
}

MediaTsMaterializedVideoConfig::MediaTsMaterializedVideoConfig(
    MediaTsVideoElementaryStreamContract contract,
    std::vector<std::vector<std::uint8_t>> parameterSetsAnnexB) noexcept
    : m_contract(std::move(contract))
    , m_parameterSetsAnnexB(std::move(parameterSetsAnnexB))
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
