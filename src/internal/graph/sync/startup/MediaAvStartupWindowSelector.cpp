#include "internal/graph/sync/startup/MediaAvStartupWindowSelector.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<std::optional<MediaAvStartupWindow>>
MediaAvStartupWindowSelector::select(
    const MediaAvStartupCoverageIndex& videoIndex,
    const MediaAvStartupCoverageIndex& audioIndex,
    const MediaAvStartupConfig& config,
    MediaAvStartupSelectionWork& work)
{
    const auto& video = videoIndex.units();
    const auto& audio = audioIndex.units();
    std::size_t audioCursor = 0;
    for (const auto& videoCandidate : video) {
        ++work.candidateOperations;
        if (config.requireVideoKeyFrame && !videoCandidate.unit->keyFrame) continue;
        auto minimumAudio = videoCandidate.unit->presentationTime->checkedSubtract(
            config.maximumInitialSkew);
        auto maximumAudio = videoCandidate.unit->presentationTime->checkedAdd(
            config.maximumInitialSkew);
        auto minimumVideoCoverage = videoCandidate.unit->presentationTime->checkedAdd(
            config.preroll);
        if (!minimumAudio || !maximumAudio || !minimumVideoCoverage) {
            return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "startup window bound arithmetic overflow"));
        }
        if (videoCandidate.coverageEnd < minimumVideoCoverage.value()) continue;
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
            if (!config.trimAudioToCommonStart && audioStart < videoStart) {
                ++audioCursor;
                continue;
            }
            const auto sourceStart = config.trimAudioToCommonStart
                ? std::max(videoStart, audioStart)
                : videoStart;
            auto audioEnd = audioCandidate.unit->presentationTime->checkedAdd(
                audioCandidate.unit->duration);
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
            if (config.trimAudioToCommonStart) {
                auto audioTrim = sourceStart.checkedSubtract(audioStart);
                if (!audioTrim) {
                    return ::media::Result<std::optional<MediaAvStartupWindow>>::failure(
                        audioTrim.error());
                }
                if (audioTrim.value() > config.maximumAudioTrim) {
                    ++audioCursor;
                    continue;
                }
            }
            if (videoCandidate.coverageEnd < requiredEnd.value()) break;
            if (audioCandidate.coverageEnd >= requiredEnd.value()) {
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
