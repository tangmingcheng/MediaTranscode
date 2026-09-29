#include "internal/graph/planner/realtime/MediaRtpSourcePreflight.h"

#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<int> remainingRawRtpStartupMilliseconds(
    std::chrono::steady_clock::time_point deadline,
    const char* phase)
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return ::media::Result<int>::failure(::media::ErrorInfo::wouldBlock(
            std::string("raw RTP preflight reached total open timeout during ") +
            phase));
    }
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
        deadline - now);
    if (remaining.count() <= 0 ||
        remaining.count() > (std::numeric_limits<int>::max)()) {
        return ::media::Result<int>::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP preflight deadline is outside the supported range"));
    }
    return ::media::Result<int>::success(
        static_cast<int>(remaining.count()));
}

} // namespace

MediaRtpSourcePreflight::MediaRtpSourcePreflight(MediaPreparedRawRtpProbe probe,
    std::chrono::steady_clock::time_point deadline)
    : m_probe(std::move(probe)), m_deadline(deadline) {}

MediaRtpSourcePreflight::MediaRtpSourcePreflight(MediaRtpSourcePreflight&& other) noexcept
    : m_probe(std::exchange(other.m_probe, std::nullopt)), m_deadline(other.m_deadline) {}

::media::Result<MediaRtpSourcePreflight> MediaRtpSourcePreflight::begin(
    const MediaRealtimeInputConfig& input, MediaTranscodeStreamSet streamSet,
    std::chrono::steady_clock::time_point deadline)
{
    using Result = ::media::Result<MediaRtpSourcePreflight>;
    if (input.type != RealtimeInputType::RtpPort || !input.openTimeoutMs ||
        *input.openTimeoutMs <= 0 ||
        (streamSet != MediaTranscodeStreamSet::VideoOnly &&
         streamSet != MediaTranscodeStreamSet::AudioVideo))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP source preflight requires explicit input type, timeout and stream set"));
    auto remaining = remainingRawRtpStartupMilliseconds(deadline, "video signaling detection");
    if (!remaining) return Result::failure(remaining.error());
    if (remaining.value() > *input.openTimeoutMs)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP source preflight deadline exceeds the input open timeout"));
    auto probeInput = input;
    probeInput.openTimeoutMs = remaining.value();
    auto probe = MediaRealtimeInputPlanner::prepareRawRtpVideo(probeInput, streamSet);
    if (!probe) return Result::failure(probe.error());
    auto afterProbe = remainingRawRtpStartupMilliseconds(deadline, "video signaling detection");
    if (!afterProbe) return Result::failure(afterProbe.error());
    if ((streamSet == MediaTranscodeStreamSet::AudioVideo) !=
        std::holds_alternative<MediaPreparedRawRtpAudioVideoProbe>(probe.value()))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP prepared probe stream set conflicts with request"));
    return Result::success(MediaRtpSourcePreflight(std::move(probe).value(), deadline));
}

::media::Result<MediaRtpSourceObservation> MediaRtpSourcePreflight::observation() const
{
    using Result = ::media::Result<MediaRtpSourceObservation>;
    if (!m_probe) return Result::failure(::media::ErrorInfo::notInitialized(
        "RTP source preflight was already consumed"));
    return std::visit([](const auto& probe) {
        return Result::success(MediaRtpSourceObservation{probe.signaling, probe.sourceFrameRate});
    }, *m_probe);
}

::media::Result<MediaPreparedRtpSource> MediaRtpSourcePreflight::seal() &&
{
    using Result = ::media::Result<MediaPreparedRtpSource>;
    auto owned = std::exchange(m_probe, std::nullopt);
    if (!owned) return Result::failure(::media::ErrorInfo::notInitialized(
        "RTP source preflight was already consumed"));
    auto beforeSeal = remainingRawRtpStartupMilliseconds(m_deadline, "input capture sealing");
    if (!beforeSeal) return Result::failure(beforeSeal.error());
    if (auto status = MediaRawRtpInputPreparer::sealPreflight(*owned); !status)
        return Result::failure(status.error());
    return std::visit([&](auto& probe) -> Result {
        auto videoIngress = MediaPreparedRtpIngressPlanner::plan(probe.video);
        if (!videoIngress) return Result::failure(videoIngress.error());
        std::optional<MediaRtpIngressPlan> audioIngress;
        if constexpr (requires { probe.audio; }) {
            auto planned = MediaPreparedRtpIngressPlanner::plan(probe.audio);
            if (!planned) return Result::failure(planned.error());
            audioIngress.emplace(std::move(planned).value());
        }
        auto afterSeal = remainingRawRtpStartupMilliseconds(m_deadline, "input ingress planning");
        if (!afterSeal) return Result::failure(afterSeal.error());
        return Result::success(MediaPreparedRtpSource(std::move(*owned),
            std::move(videoIngress).value(), std::move(audioIngress), m_deadline));
    }, *owned);
}

MediaPreparedRtpSource::MediaPreparedRtpSource(MediaPreparedRawRtpProbe probe,
    MediaRtpIngressPlan videoIngress, std::optional<MediaRtpIngressPlan> audioIngress,
    std::chrono::steady_clock::time_point deadline)
    : m_probe(std::move(probe)), m_videoIngress(std::move(videoIngress)),
      m_audioIngress(std::move(audioIngress)), m_deadline(deadline) {}

MediaPreparedRtpSource::MediaPreparedRtpSource(MediaPreparedRtpSource&& other) noexcept
    : m_probe(std::exchange(other.m_probe, std::nullopt)),
      m_videoIngress(std::move(other.m_videoIngress)),
      m_audioIngress(std::move(other.m_audioIngress)), m_deadline(other.m_deadline) {}

::media::Result<MediaPreparedRtpSourceView> MediaPreparedRtpSource::resources() const
{
    using Result = ::media::Result<MediaPreparedRtpSourceView>;
    if (!m_probe) return Result::failure(::media::ErrorInfo::notInitialized(
        "Prepared RTP source was already consumed"));
    return std::visit([&](const auto& probe) {
        const MediaPreparedRealtimeInput* audio = nullptr;
        if constexpr (requires { probe.audio; }) audio = &probe.audio;
        return Result::success(MediaPreparedRtpSourceView{
            probe.video, audio, m_videoIngress, m_audioIngress ? &*m_audioIngress : nullptr});
    }, *m_probe);
}

::media::Result<MediaPreparedRtpSourceInputs> MediaPreparedRtpSource::release() &&
{
    using Result = ::media::Result<MediaPreparedRtpSourceInputs>;
    auto owned = std::exchange(m_probe, std::nullopt);
    if (!owned) return Result::failure(::media::ErrorInfo::notInitialized(
        "Prepared RTP source was already consumed"));
    auto beforeRelease = remainingRawRtpStartupMilliseconds(m_deadline, "resolved product planning");
    if (!beforeRelease) return Result::failure(beforeRelease.error());
    return std::visit([&](auto& probe) -> Result {
        if (auto status = probe.video.configureRawRtpRuntimeIngress(m_videoIngress); !status)
            return Result::failure(status.error());
        std::optional<MediaPreparedRealtimeInput> audio;
        if constexpr (requires { probe.audio; }) {
            if (!m_audioIngress) return Result::failure(::media::ErrorInfo::notInitialized(
                "Prepared audio input requires its sealed ingress plan"));
            if (auto status = probe.audio.configureRawRtpRuntimeIngress(*m_audioIngress); !status)
                return Result::failure(status.error());
            audio.emplace(std::move(probe.audio));
        }
        auto afterRelease = remainingRawRtpStartupMilliseconds(m_deadline, "input resource handoff");
        if (!afterRelease) return Result::failure(afterRelease.error());
        return Result::success(MediaPreparedRtpSourceInputs{std::move(probe.video), std::move(audio)});
    }, *owned);
}

} // namespace media::ffmpeg::graph
