#include "internal/graph/protocol/mpegts/MediaTsPreflightDurationProbe.h"

#include <string>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaTsPreflightDurationProbe>
MediaTsPreflightDurationProbe::create(
    MediaTsRuntimeBinding binding,
    std::size_t frameLimit)
{
    const std::size_t capacity = std::visit(
        [](const auto& selected) {
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsVideoOnlyRuntimeBinding>) {
                return selected.videoPesProvenanceCapacity;
            } else {
                return selected.pesProvenanceCapacity;
            }
        },
        binding);
    if (frameLimit == 0 ||
        !MediaTsRuntimeBindingCodec::validate(binding, capacity)) {
        return ::media::Result<MediaTsPreflightDurationProbe>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS duration probe requires a complete typed binding and positive frame limit"));
    }
    return ::media::Result<MediaTsPreflightDurationProbe>::success(
        MediaTsPreflightDurationProbe(
            std::move(binding), frameLimit));
}

MediaTsPreflightDurationProbe::MediaTsPreflightDurationProbe(
    MediaTsRuntimeBinding binding,
    std::size_t frameLimit) noexcept
    : m_binding(std::move(binding)), m_frameLimit(frameLimit)
{
}

::media::Result<std::optional<MediaTsSelectedPacketDurationEvidence>>
MediaTsPreflightDurationProbe::buffer(MediaTsReadFrameEnvelope envelope)
{
    const bool audioVideo = std::holds_alternative<
        MediaTsAudioVideoRuntimeBinding>(m_binding);
    if (m_videoEvidence && (!audioVideo || m_audioEvidence)) {
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS duration probe is already complete"));
    }
    if (envelope.state == MediaTsReadFrameState::Discarded) {
        ++m_observedFrameCount;
        if (m_observedFrameCount >= m_frameLimit) {
            return ::media::Result<
                std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MPEG-TS duration probe frame limit reached without selected packet duration evidence"));
        }
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::success(
                std::nullopt);
    }
    if (envelope.state != MediaTsReadFrameState::Frame || !envelope.packet) {
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS duration probe ended before every selected stream published packet duration"));
    }

    const auto video = std::visit(
        [](const auto& selected) { return selected.video; }, m_binding);
    const auto audio = std::get_if<MediaTsAudioVideoRuntimeBinding>(&m_binding);
    const int streamIndex = envelope.packet->stream_index;
    const std::int64_t duration = envelope.packet->duration;
    ++m_observedFrameCount;
    m_replay.push_back(std::move(envelope));
    if (streamIndex == video.streamIndex && !m_videoEvidence) {
        if (duration <= 0) {
            return ::media::Result<
                std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "selected MPEG-TS video packet duration must be positive"));
        }
        m_videoEvidence = MediaTsPacketDurationEvidence{
            video.streamIndex, video.pid, duration, video.timeBase};
    } else if (audio && streamIndex == audio->audio.streamIndex &&
               !m_audioEvidence) {
        if (duration <= 0) {
            return ::media::Result<
                std::optional<MediaTsSelectedPacketDurationEvidence>>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "selected MPEG-TS audio packet duration must be positive"));
        }
        m_audioEvidence = MediaTsPacketDurationEvidence{
            audio->audio.streamIndex, audio->audio.pid, duration,
            audio->audio.timeBase};
    }

    if (m_videoEvidence && !audioVideo) {
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::success(
                MediaTsVideoOnlyPacketDurationEvidence{*m_videoEvidence});
    }
    if (m_videoEvidence && m_audioEvidence) {
        return ::media::Result<
            std::optional<MediaTsSelectedPacketDurationEvidence>>::success(
                MediaTsAudioVideoPacketDurationEvidence{
                    *m_videoEvidence, *m_audioEvidence});
    }
    if (m_observedFrameCount >= m_frameLimit) {
        const std::string missing = m_videoEvidence && audioVideo
            ? "audio"
            : "video";
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
