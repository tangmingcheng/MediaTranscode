#include "internal/graph/protocol/mpegts/MediaTsPreflightDurationProbe.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool validBinding(const MediaTsRuntimeStreamBinding& binding) noexcept
{
    return binding.streamIndex >= 0 && binding.pid > 0 && binding.pid < 0x1FFF;
}

bool validTimeBase(const MediaRational& timeBase) noexcept
{
    return timeBase.num > 0 && timeBase.den > 0;
}

} // namespace

::media::Result<MediaTsPreflightDurationProbe>
MediaTsPreflightDurationProbe::create(
    MediaTsRuntimeStreamBinding video,
    MediaRational videoTimeBase,
    MediaTsRuntimeStreamBinding audio,
    MediaRational audioTimeBase,
    std::size_t frameLimit)
{
    if (!validBinding(video) || !validBinding(audio) ||
        video.streamIndex == audio.streamIndex || video.pid == audio.pid ||
        !validTimeBase(videoTimeBase) || !validTimeBase(audioTimeBase) ||
        frameLimit == 0) {
        return ::media::Result<MediaTsPreflightDurationProbe>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS duration probe requires distinct selected streams, "
                "positive time bases, and a positive frame limit"));
    }
    return ::media::Result<MediaTsPreflightDurationProbe>::success(
        MediaTsPreflightDurationProbe(
            video, videoTimeBase, audio, audioTimeBase, frameLimit));
}

MediaTsPreflightDurationProbe::MediaTsPreflightDurationProbe(
    MediaTsRuntimeStreamBinding video,
    MediaRational videoTimeBase,
    MediaTsRuntimeStreamBinding audio,
    MediaRational audioTimeBase,
    std::size_t frameLimit) noexcept
    : m_video(video), m_videoTimeBase(videoTimeBase), m_audio(audio),
      m_audioTimeBase(audioTimeBase), m_frameLimit(frameLimit)
{
}

::media::Result<std::optional<MediaTsSelectedPacketDurationEvidence>>
MediaTsPreflightDurationProbe::buffer(MediaTsReadFrameEnvelope envelope)
{
    if (m_videoEvidence && m_audioEvidence) {
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS duration probe is already complete"));
    }
    if (envelope.state != MediaTsReadFrameState::Frame || !envelope.packet) {
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS duration probe ended before both selected streams "
                    "published packet duration"));
    }

    const int streamIndex = envelope.packet->stream_index;
    const std::int64_t duration = envelope.packet->duration;
    m_replay.push_back(std::move(envelope));
    if (streamIndex == m_video.streamIndex && !m_videoEvidence) {
        if (duration <= 0) {
            return ::media::Result<
                std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "selected MPEG-TS video packet duration must be positive"));
        }
        m_videoEvidence = MediaTsPacketDurationEvidence{
            m_video.streamIndex, m_video.pid, duration, m_videoTimeBase};
    } else if (streamIndex == m_audio.streamIndex && !m_audioEvidence) {
        if (duration <= 0) {
            return ::media::Result<
                std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "selected MPEG-TS audio packet duration must be positive"));
        }
        m_audioEvidence = MediaTsPacketDurationEvidence{
            m_audio.streamIndex, m_audio.pid, duration, m_audioTimeBase};
    }

    if (m_videoEvidence && m_audioEvidence) {
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::success(
                MediaTsSelectedPacketDurationEvidence{
                    *m_videoEvidence, *m_audioEvidence});
    }
    if (m_replay.size() >= m_frameLimit) {
        const std::string missing = m_videoEvidence ? "audio" : "video";
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS duration probe frame limit reached without " +
                    missing + " packet duration evidence"));
    }
    return ::media::Result<
        std::optional<MediaTsSelectedPacketDurationEvidence>>::success(
            std::nullopt);
}

::media::Result<MediaTsReadFrameEnvelope>
MediaTsPreflightDurationProbe::popReplay()
{
    if (m_replay.empty()) {
        return ::media::Result<MediaTsReadFrameEnvelope>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS preflight replay is exhausted"));
    }
    auto envelope = std::move(m_replay.front());
    m_replay.pop_front();
    return ::media::Result<MediaTsReadFrameEnvelope>::success(
        std::move(envelope));
}

} // namespace media::ffmpeg::graph
