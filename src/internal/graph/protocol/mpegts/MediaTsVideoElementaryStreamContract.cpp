#include "internal/graph/protocol/mpegts/MediaTsVideoElementaryStreamContract.h"

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint8_t H264StreamType = 0x1B;
constexpr std::uint8_t HevcStreamType = 0x24;

bool validCodecStreamType(MediaTsVideoCodec codec,
                          std::uint8_t streamType) noexcept
{
    switch (codec) {
    case MediaTsVideoCodec::H264:
        return streamType == H264StreamType;
    case MediaTsVideoCodec::Hevc:
        return streamType == HevcStreamType;
    }
    return false;
}

} // namespace

::media::Result<MediaTsVideoElementaryStreamContract>
MediaTsVideoElementaryStreamContract::create(
    MediaTsVideoCodec codec,
    MediaTsNalLayout layout,
    std::uint8_t nalLengthBytes,
    std::uint8_t streamType)
{
    if (!validCodecStreamType(codec, streamType)) {
        return ::media::Result<MediaTsVideoElementaryStreamContract>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video codec and PMT stream type conflict"));
    }
    switch (layout) {
    case MediaTsNalLayout::AnnexB:
        if (nalLengthBytes != 0) {
            return ::media::Result<MediaTsVideoElementaryStreamContract>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS Annex-B video contract cannot carry a NAL length width"));
        }
        break;
    case MediaTsNalLayout::LengthPrefixed:
        if (nalLengthBytes < 1 || nalLengthBytes > 4) {
            return ::media::Result<MediaTsVideoElementaryStreamContract>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS length-prefixed video contract requires a NAL length width from one through four bytes"));
        }
        break;
    default:
        return ::media::Result<MediaTsVideoElementaryStreamContract>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video contract has an unknown NAL layout"));
    }
    return ::media::Result<MediaTsVideoElementaryStreamContract>::success(
        MediaTsVideoElementaryStreamContract(
            codec, layout, nalLengthBytes, streamType));
}

MediaTsVideoElementaryStreamContract::
MediaTsVideoElementaryStreamContract(
    MediaTsVideoCodec codec,
    MediaTsNalLayout layout,
    std::uint8_t nalLengthBytes,
    std::uint8_t streamType) noexcept
    : m_codec(codec)
    , m_layout(layout)
    , m_nalLengthBytes(nalLengthBytes)
    , m_streamType(streamType)
{
}

} // namespace media::ffmpeg::graph
