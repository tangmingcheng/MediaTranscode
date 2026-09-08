#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaPreparedInputPayloadSource : std::uint8_t {
    GenericDemuxPacket = 1,
    MpegTsPesPacket = 2,
    RawRtpAccessUnit = 3
};

struct MediaPreparedInputPayloadBound final {
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    std::uint64_t maximumPayloadBytes = 0;
    std::string authority;

    bool valid() const noexcept;
};

struct MediaPreparedInputPayloadEnvelope final {
    MediaPreparedInputPayloadSource source =
        MediaPreparedInputPayloadSource::GenericDemuxPacket;
    std::uint64_t maximumPayloadsPerInputCompletion = 0;
    std::string completionAuthority;
    std::vector<MediaPreparedInputPayloadBound> streams;

    ::media::Status validate() const;
    const MediaPreparedInputPayloadBound* find(
        MediaStreamKind streamKind) const noexcept;
};

} // namespace media::ffmpeg::graph
