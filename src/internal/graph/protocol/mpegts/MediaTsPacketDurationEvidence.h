#pragma once

#include "internal/graph/model/MediaGraphTypes.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaTsPacketDurationEvidence final {
    int streamIndex;
    std::uint16_t elementaryPid;
    std::int64_t packetDuration;
    MediaRational timeBase;
    friend bool operator==(const MediaTsPacketDurationEvidence& left,
                           const MediaTsPacketDurationEvidence& right) noexcept
    {
        return left.streamIndex == right.streamIndex &&
            left.elementaryPid == right.elementaryPid &&
            left.packetDuration == right.packetDuration &&
            left.timeBase.num == right.timeBase.num &&
            left.timeBase.den == right.timeBase.den;
    }
};

struct MediaTsSelectedPacketDurationEvidence final {
    MediaTsPacketDurationEvidence video;
    MediaTsPacketDurationEvidence audio;
    friend bool operator==(const MediaTsSelectedPacketDurationEvidence&,
                           const MediaTsSelectedPacketDurationEvidence&) = default;
};

} // namespace media::ffmpeg::graph
