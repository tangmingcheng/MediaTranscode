#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramContractValidator.h"

#include <algorithm>

namespace media::ffmpeg::graph {
namespace {

std::optional<int> streamPid(
    const FFmpegInputProgramSnapshot& program,
    int streamIndex) noexcept
{
    const auto found = std::find_if(program.streamBindings.begin(), program.streamBindings.end(),
        [streamIndex](const auto& binding) { return binding.streamIndex == streamIndex; });
    if (found == program.streamBindings.end()) return std::nullopt;
    return found->elementaryPid;
}

struct SelectedProgramCandidate final {
    int programNumber;
    int programMapPid;
    int pcrPid;
    int videoPid;
    std::optional<int> audioPid;
};

::media::Result<SelectedProgramCandidate> selectProgramCandidate(
    const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
    const MediaTsProgramInventorySnapshot& parserInventory,
    int selectedVideoStream,
    std::optional<int> selectedAudioStream)
{
    if (auto snapshots = MediaTsProgramContractValidator::validateSnapshots(
            publicPrograms, parserInventory);
        !snapshots) {
        return ::media::Result<SelectedProgramCandidate>::failure(
            snapshots.error());
    }
    std::optional<SelectedProgramCandidate> selected;
    for (const auto& publicProgram : publicPrograms) {
        const auto videoPid = streamPid(publicProgram, selectedVideoStream);
        const auto audioPid = selectedAudioStream
            ? streamPid(publicProgram, *selectedAudioStream)
            : std::optional<int>{};
        if (!videoPid || (selectedAudioStream && !audioPid)) continue;

        if ((audioPid && *videoPid == *audioPid) || selected) {
            return ::media::Result<SelectedProgramCandidate>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS selected streams do not identify exactly one complete program"));
        }
        selected = SelectedProgramCandidate{
            publicProgram.programNumber,
            publicProgram.pmtPid,
            publicProgram.pcrPid,
            *videoPid,
            audioPid};
    }
    if (!selected) {
        return ::media::Result<SelectedProgramCandidate>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS selected streams are absent from a complete program"));
    }
    return ::media::Result<SelectedProgramCandidate>::success(*selected);
}

} // namespace

::media::Result<MediaTsVideoOnlyProgramSelection>
MediaTsProgramSelector::selectVideoOnly(
    const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
    const MediaTsProgramInventorySnapshot& parserInventory,
    int selectedVideoStream,
    MediaRational videoTimeBase)
{
    if (selectedVideoStream < 0 ||
        !MediaTsProgramContractValidator::isSelectedStreamTimeBase(
            videoTimeBase)) {
        return ::media::Result<MediaTsVideoOnlyProgramSelection>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS selected video stream facts are invalid"));
    }

    auto selected = selectProgramCandidate(
        publicPrograms, parserInventory, selectedVideoStream, std::nullopt);
    if (!selected) {
        return ::media::Result<MediaTsVideoOnlyProgramSelection>::failure(
            selected.error());
    }
    return ::media::Result<MediaTsVideoOnlyProgramSelection>::success(
        MediaTsVideoOnlyProgramSelection(
            selected.value().programNumber,
            selected.value().programMapPid,
            selected.value().pcrPid,
            MediaTsSelectedStreamPlan(
                selectedVideoStream,
                selected.value().videoPid,
                videoTimeBase)));
}

::media::Result<MediaTsAudioVideoProgramSelection>
MediaTsProgramSelector::selectAudioVideo(
    const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
    const MediaTsProgramInventorySnapshot& parserInventory,
    int selectedVideoStream,
    MediaRational videoTimeBase,
    int selectedAudioStream,
    MediaRational audioTimeBase)
{
    if (selectedVideoStream < 0 || selectedAudioStream < 0 ||
        selectedVideoStream == selectedAudioStream ||
        !MediaTsProgramContractValidator::isSelectedStreamTimeBase(
            videoTimeBase) ||
        !MediaTsProgramContractValidator::isSelectedStreamTimeBase(
            audioTimeBase)) {
        return ::media::Result<MediaTsAudioVideoProgramSelection>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS selected A/V stream facts are invalid"));
    }

    auto selected = selectProgramCandidate(
        publicPrograms, parserInventory, selectedVideoStream,
        selectedAudioStream);
    if (!selected) {
        return ::media::Result<MediaTsAudioVideoProgramSelection>::failure(
            selected.error());
    }
    return ::media::Result<MediaTsAudioVideoProgramSelection>::success(
        MediaTsAudioVideoProgramSelection(
            selected.value().programNumber,
            selected.value().programMapPid,
            selected.value().pcrPid,
            MediaTsSelectedStreamPlan(
                selectedVideoStream,
                selected.value().videoPid,
                videoTimeBase),
            MediaTsSelectedStreamPlan(
                selectedAudioStream,
                *selected.value().audioPid,
                audioTimeBase)));
}

} // namespace media::ffmpeg::graph
