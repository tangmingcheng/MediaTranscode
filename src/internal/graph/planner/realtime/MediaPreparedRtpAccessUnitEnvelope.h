#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/planner/realtime/MediaPreparedInputPayloadEnvelope.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaPreparedRtpAccessUnitEnvelope final {
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    std::string codecName;
    std::uint64_t maximumAccessUnitBytes = 0;
    std::uint64_t maximumAccessUnitsPerPush = 0;
    std::string sizeAuthority;
    std::string completionAuthority;

    ::media::Status validate() const;
    MediaPreparedInputPayloadEnvelope asInputPayloadEnvelope() const;
};

class MediaPreparedRtpAccessUnitEnvelopePlanner final {
public:
    static ::media::Result<MediaPreparedRtpAccessUnitEnvelope> plan(
        const MediaRtpDepacketizerConfig& depacketizer,
        std::uint64_t maximumDatagramBytes);

private:
    MediaPreparedRtpAccessUnitEnvelopePlanner() = delete;
};

} // namespace media::ffmpeg::graph
