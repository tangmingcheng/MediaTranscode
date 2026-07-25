#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketDurationEvidence.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

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
