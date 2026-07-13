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

std::optional<std::size_t> streamPosition(
    const FFmpegInputProgramSnapshot& program,
    int streamIndex) noexcept
{
    const auto found = std::find(program.streamIndexes.begin(),
                                 program.streamIndexes.end(), streamIndex);
    if (found == program.streamIndexes.end()) return std::nullopt;
    return static_cast<std::size_t>(found - program.streamIndexes.begin());
}

} // namespace

::media::Result<MediaTsSelectedProgramPlan> MediaTsProgramSelector::select(
    const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
    const MediaTsProgramInventorySnapshot& parserInventory,
    int selectedVideoStream,
    int selectedAudioStream)
{
    if (selectedVideoStream < 0 || selectedAudioStream < 0 ||
        selectedVideoStream == selectedAudioStream) {
        return ::media::Result<MediaTsSelectedProgramPlan>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS selected stream indexes are invalid"));
    }

    std::optional<MediaTsSelectedProgramPlan> selected;
    for (const auto& publicProgram : publicPrograms) {
        const auto videoPosition = streamPosition(publicProgram, selectedVideoStream);
        const auto audioPosition = streamPosition(publicProgram, selectedAudioStream);
        if (!videoPosition || !audioPosition) continue;

        const MediaTsProgramInfo* parserProgram = inventoryProgram(
            parserInventory, publicProgram.programNumber);
        if (!parserProgram || publicProgram.programNumber <= 0 ||
            publicProgram.pmtPid <= 0 || publicProgram.pcrPid <= 0 ||
            parserProgram->pmtPid != publicProgram.pmtPid ||
            parserProgram->pcrPid != publicProgram.pcrPid ||
            parserProgram->elementaryStreams.size() != publicProgram.streamIndexes.size() ||
            *videoPosition >= parserProgram->elementaryStreams.size() ||
            *audioPosition >= parserProgram->elementaryStreams.size()) {
            return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS public program and parser inventory mismatch"));
        }

        MediaTsSelectedProgramPlan candidate;
        candidate.programNumber = publicProgram.programNumber;
        candidate.programMapPid = publicProgram.pmtPid;
        candidate.pcrPid = publicProgram.pcrPid;
        candidate.videoPid = parserProgram->elementaryStreams[*videoPosition].pid;
        candidate.audioPid = parserProgram->elementaryStreams[*audioPosition].pid;
        if (candidate.videoPid <= 0 || candidate.audioPid <= 0 ||
            candidate.videoPid == candidate.audioPid || selected) {
            return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS selected streams do not identify exactly one complete program"));
        }
        selected = candidate;
    }

    if (!selected) {
        return ::media::Result<MediaTsSelectedProgramPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS selected streams are absent from a complete program"));
    }
    return ::media::Result<MediaTsSelectedProgramPlan>::success(*selected);
}

} // namespace media::ffmpeg::graph
