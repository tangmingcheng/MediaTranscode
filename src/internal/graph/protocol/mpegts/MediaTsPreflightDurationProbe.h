#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

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

class MediaTsPreflightDurationProbe final {
public:
    static ::media::Result<MediaTsPreflightDurationProbe> create(
        MediaTsRuntimeStreamBinding video,
        MediaRational videoTimeBase,
        MediaTsRuntimeStreamBinding audio,
        MediaRational audioTimeBase,
        std::size_t frameLimit);

    MediaTsPreflightDurationProbe(const MediaTsPreflightDurationProbe&) = delete;
    MediaTsPreflightDurationProbe& operator=(
        const MediaTsPreflightDurationProbe&) = delete;
    MediaTsPreflightDurationProbe(MediaTsPreflightDurationProbe&&) noexcept = default;
    MediaTsPreflightDurationProbe& operator=(
        MediaTsPreflightDurationProbe&&) noexcept = default;

    ::media::Result<std::optional<MediaTsSelectedPacketDurationEvidence>> buffer(
        MediaTsReadFrameEnvelope envelope);
    ::media::Result<MediaTsReadFrameEnvelope> popReplay();
    bool replayEmpty() const noexcept { return m_replay.empty(); }

private:
    MediaTsPreflightDurationProbe(
        MediaTsRuntimeStreamBinding video,
        MediaRational videoTimeBase,
        MediaTsRuntimeStreamBinding audio,
        MediaRational audioTimeBase,
        std::size_t frameLimit) noexcept;

    MediaTsRuntimeStreamBinding m_video;
    MediaRational m_videoTimeBase;
    MediaTsRuntimeStreamBinding m_audio;
    MediaRational m_audioTimeBase;
    std::size_t m_frameLimit;
    std::deque<MediaTsReadFrameEnvelope> m_replay;
    std::optional<MediaTsPacketDurationEvidence> m_videoEvidence;
    std::optional<MediaTsPacketDurationEvidence> m_audioEvidence;
};

} // namespace media::ffmpeg::graph
