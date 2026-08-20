#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaTsVideoCodec : std::uint8_t {
    H264 = 0,
    Hevc = 1
};

enum class MediaTsNalLayout : std::uint8_t {
    AnnexB = 0,
    LengthPrefixed = 1
};

class MediaTsVideoElementaryStreamContract final {
public:
    static ::media::Result<MediaTsVideoElementaryStreamContract> create(
        MediaTsVideoCodec codec,
        MediaTsNalLayout layout,
        std::uint8_t nalLengthBytes,
        std::uint8_t streamType);

    MediaTsVideoCodec codec() const noexcept { return m_codec; }
    MediaTsNalLayout layout() const noexcept { return m_layout; }
    std::uint8_t nalLengthBytes() const noexcept { return m_nalLengthBytes; }
    std::uint8_t streamType() const noexcept { return m_streamType; }

    friend bool operator==(
        const MediaTsVideoElementaryStreamContract&,
        const MediaTsVideoElementaryStreamContract&) = default;

private:
    MediaTsVideoElementaryStreamContract(
        MediaTsVideoCodec codec,
        MediaTsNalLayout layout,
        std::uint8_t nalLengthBytes,
        std::uint8_t streamType) noexcept;

    MediaTsVideoCodec m_codec;
    MediaTsNalLayout m_layout;
    std::uint8_t m_nalLengthBytes;
    std::uint8_t m_streamType;
};

} // namespace media::ffmpeg::graph
