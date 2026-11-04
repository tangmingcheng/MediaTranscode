#include "internal/graph/sync/startup/MediaAvStartupWindowSelector.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace media::ffmpeg::graph {

::media::Result<std::optional<MediaAvStartupWindow>>
MediaAvStartupWindowSelector::select(
    const MediaAvStartupReadinessIndex& videoIndex,
    const MediaAvStartupReadinessIndex* audioIndex,
    const MediaAvStartupConfig& config,
    MediaAvStartupSelectionWork& work)
{
    const auto& video = videoIndex.units();
    if ((config.members != MediaTranscodeStreamSet::VideoOnly &&
         config.members != MediaTranscodeStreamSet::AudioVideo) ||
        (config.members == MediaTranscodeStreamSet::AudioVideo) != config.audio.has_value() ||
        config.audio.has_value() != (audioIndex != nullptr))
        return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
            ::media::ErrorInfo::invalidArgument("startup coverage does not match planned members"));
    const bool display = config.videoReadiness ==
        MediaVideoStartupReadiness::DecodedPresentationSpan;
    const auto frontier = [](const MediaAvStartupReadinessEvidence& evidence) {
        return std::visit([](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, MediaAvStartupIntervalFrontier>) return value.end;
            else return value.lastPresentation;
        }, evidence);
    };
    for (const auto& candidate : video)
        if (std::holds_alternative<MediaAvStartupDisplayFrontier>(candidate.readiness) != display)
            return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                ::media::ErrorInfo::invalidArgument("Video readiness evidence differs from its plan"));
    if (audioIndex) {
        for (const auto& candidate : audioIndex->units())
            if (!std::holds_alternative<MediaAvStartupIntervalFrontier>(candidate.readiness) ||
                !std::holds_alternative<MediaRunningTime>(candidate.unit->timing))
                return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                    ::media::ErrorInfo::invalidArgument("Audio readiness requires sample intervals"));
    }
    std::size_t audioCursor = 0;
    for (const auto& videoCandidate : video) {
        ++work.candidateOperations;
        if (config.requireVideoKeyFrame && !videoCandidate.unit->keyFrame) continue;
        auto minimumVideoCoverage = videoCandidate.unit->presentationTime->checkedAdd(config.preroll);
        if (!minimumVideoCoverage)
            return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                minimumVideoCoverage.error());
        if (!audioIndex) {
            if (frontier(videoCandidate.readiness) < minimumVideoCoverage.value()) continue;
            return ::media::Result<std::optional<MediaAvStartupWindow>>::success(
                MediaAvStartupWindow{videoCandidate.unit, nullptr,
                                     *videoCandidate.unit->presentationTime});
        }
        const auto& audio = audioIndex->units();
        auto minimumAudio = videoCandidate.unit->presentationTime->checkedSubtract(
            config.audio->maximumInitialSkew);
        auto maximumAudio = videoCandidate.unit->presentationTime->checkedAdd(
            config.audio->maximumInitialSkew);
        if (!minimumAudio || !maximumAudio)
            return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                ::media::ErrorInfo::invalidArgument("startup window bound arithmetic overflow"));
        if (frontier(videoCandidate.readiness) < minimumVideoCoverage.value()) continue;
        while (audioCursor < audio.size() &&
               *audio[audioCursor].unit->presentationTime < minimumAudio.value()) {
            ++work.candidateOperations;
            ++audioCursor;
        }
        while (audioCursor < audio.size() &&
               *audio[audioCursor].unit->presentationTime <= maximumAudio.value()) {
            ++work.candidateOperations;
            const auto& audioCandidate = audio[audioCursor];
            const auto videoStart = *videoCandidate.unit->presentationTime;
            const auto audioStart = *audioCandidate.unit->presentationTime;
            if (!config.audio->trimToCommonStart && audioStart < videoStart) {
                ++audioCursor;
                continue;
            }
            const auto sourceStart = config.audio->trimToCommonStart
                ? std::max(videoStart, audioStart)
                : videoStart;
            auto audioEnd = audioCandidate.unit->presentationTime->checkedAdd(
                std::get<MediaRunningTime>(audioCandidate.unit->timing));
            auto requiredEnd = sourceStart.checkedAdd(config.preroll);
            if (!audioEnd || !requiredEnd) {
                return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "startup candidate arithmetic overflow"));
            }
            if (audioEnd.value() <= sourceStart) {
                ++audioCursor;
                continue;
            }
            if (config.audio->trimToCommonStart) {
                auto audioTrim = sourceStart.checkedSubtract(audioStart);
                if (!audioTrim) {
                    return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                        audioTrim.error());
                }
                if (audioTrim.value() > config.audio->maximumTrim) {
                    ++audioCursor;
                    continue;
                }
            }
            if (frontier(videoCandidate.readiness) < requiredEnd.value()) break;
            if (frontier(audioCandidate.readiness) >= requiredEnd.value()) {
                return ::media::Result<std::optional<MediaAvStartupWindow>>::success(
                    MediaAvStartupWindow{videoCandidate.unit,
                                         audioCandidate.unit,
                                         sourceStart});
            }
            ++audioCursor;
        }
    }
    return ::media::Result<std::optional<MediaAvStartupWindow>>::success(std::nullopt);
}

} // namespace media::ffmpeg::graph
