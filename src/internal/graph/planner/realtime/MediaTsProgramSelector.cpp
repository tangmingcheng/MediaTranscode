#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

const MediaTsProgramInfo* inventoryProgram(
    const MediaTsProgramInventorySnapshot& inventory,
    int programNumber) noexcept
{
    const MediaTsProgramInfo* match = nullptr;
    for (const auto& program : inventory.programs) {
        if (program.programNumber != programNumber) continue;
        if (match) return nullptr;
        match = &program;
    }
    return match;
}

std::optional<int> streamPid(
    const FFmpegInputProgramSnapshot& program,
    int streamIndex) noexcept
{
    const auto found = std::find_if(program.streamBindings.begin(), program.streamBindings.end(),
        [streamIndex](const auto& binding) { return binding.streamIndex == streamIndex; });
    if (found == program.streamBindings.end()) return std::nullopt;
    return found->elementaryPid;
}

bool validTimeBase(MediaRational timeBase) noexcept
{
    return timeBase.num > 0 && timeBase.den > 0;
}

::media::Status validatePublicProgram(
    const FFmpegInputProgramSnapshot& publicProgram,
    const MediaTsProgramInfo& parserProgram)
{
    if (publicProgram.programNumber <= 0 || publicProgram.pmtPid <= 0 ||
        publicProgram.pmtPid >= 0x1FFF || publicProgram.pcrPid <= 0 ||
        publicProgram.pcrPid >= 0x1FFF ||
        parserProgram.pmtPid != publicProgram.pmtPid ||
        parserProgram.pcrPid != publicProgram.pcrPid ||
        parserProgram.elementaryStreams.size() !=
            publicProgram.streamBindings.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS public program and parser inventory mismatch"));
    }

    for (std::size_t index = 0;
         index < publicProgram.streamBindings.size(); ++index) {
        const auto& binding = publicProgram.streamBindings[index];
        if (binding.streamIndex < 0 || binding.elementaryPid <= 0 ||
            binding.elementaryPid >= 0x1FFF) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS public stream binding is invalid"));
        }
        for (std::size_t other = index + 1;
             other < publicProgram.streamBindings.size(); ++other) {
            if (binding.streamIndex ==
                    publicProgram.streamBindings[other].streamIndex ||
                binding.elementaryPid ==
                    publicProgram.streamBindings[other].elementaryPid) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS public stream binding is duplicated"));
            }
        }
        const auto matches = std::count_if(
            parserProgram.elementaryStreams.begin(),
            parserProgram.elementaryStreams.end(),
            [&binding](const auto& stream) {
                return stream.pid == binding.elementaryPid;
            });
        if (matches != 1) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS public PID membership mismatch"));
        }
    }
    return ::media::Status::success();
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
    std::optional<SelectedProgramCandidate> selected;
    for (const auto& publicProgram : publicPrograms) {
        const auto videoPid = streamPid(publicProgram, selectedVideoStream);
        const auto audioPid = selectedAudioStream
            ? streamPid(publicProgram, *selectedAudioStream)
            : std::optional<int>{};
        if (!videoPid || (selectedAudioStream && !audioPid)) continue;

        const MediaTsProgramInfo* parserProgram = inventoryProgram(
            parserInventory, publicProgram.programNumber);
        if (!parserProgram) {
            return ::media::Result<SelectedProgramCandidate>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS public program and parser inventory mismatch"));
        }
        if (auto status = validatePublicProgram(publicProgram, *parserProgram);
            !status) {
            return ::media::Result<SelectedProgramCandidate>::failure(
                status.error());
        }
        const auto parserHas = [parserProgram](int pid) {
            return std::count_if(
                parserProgram->elementaryStreams.begin(),
                parserProgram->elementaryStreams.end(),
                [pid](const auto& stream) { return stream.pid == pid; }) == 1;
        };
        if (!parserHas(*videoPid) || (audioPid && !parserHas(*audioPid))) {
            return ::media::Result<SelectedProgramCandidate>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS public PID is absent from parser program"));
        }
        if (*videoPid <= 0 || (audioPid && *audioPid <= 0) ||
            (audioPid && *videoPid == *audioPid) || selected) {
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
    if (selectedVideoStream < 0 || !validTimeBase(videoTimeBase)) {
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
        MediaTsVideoOnlyProgramSelection{
            selected.value().programNumber,
            selected.value().programMapPid,
            selected.value().pcrPid,
            MediaTsSelectedStreamPlan{
                selectedVideoStream,
                selected.value().videoPid,
                videoTimeBase}});
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
        !validTimeBase(videoTimeBase) || !validTimeBase(audioTimeBase)) {
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
        MediaTsAudioVideoProgramSelection{
            selected.value().programNumber,
            selected.value().programMapPid,
            selected.value().pcrPid,
            MediaTsSelectedStreamPlan{
                selectedVideoStream,
                selected.value().videoPid,
                videoTimeBase},
            MediaTsSelectedStreamPlan{
                selectedAudioStream,
                *selected.value().audioPid,
                audioTimeBase}});
}

} // namespace media::ffmpeg::graph
