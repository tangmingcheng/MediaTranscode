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
        const auto videoPid = streamPid(publicProgram, selectedVideoStream);
        const auto audioPid = streamPid(publicProgram, selectedAudioStream);
        if (!videoPid || !audioPid) continue;

        const MediaTsProgramInfo* parserProgram = inventoryProgram(
            parserInventory, publicProgram.programNumber);
        if (!parserProgram || publicProgram.programNumber <= 0 ||
            publicProgram.pmtPid <= 0 || publicProgram.pcrPid <= 0 ||
            parserProgram->pmtPid != publicProgram.pmtPid ||
            parserProgram->pcrPid != publicProgram.pcrPid ||
            parserProgram->elementaryStreams.size() != publicProgram.streamBindings.size()) {
            return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS public program and parser inventory mismatch"));
        }

        for (std::size_t index = 0; index < publicProgram.streamBindings.size(); ++index) {
            const auto& binding = publicProgram.streamBindings[index];
            if (binding.streamIndex < 0 || binding.elementaryPid <= 0 ||
                binding.elementaryPid > 0x1fff) {
                return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                    ::media::ErrorInfo::invalidArgument("MPEG-TS public stream binding is invalid"));
            }
            for (std::size_t other = index + 1; other < publicProgram.streamBindings.size(); ++other) {
                if (binding.streamIndex == publicProgram.streamBindings[other].streamIndex ||
                    binding.elementaryPid == publicProgram.streamBindings[other].elementaryPid) {
                    return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                        ::media::ErrorInfo::invalidArgument("MPEG-TS public stream binding is duplicated"));
                }
            }
            const auto matches = std::count_if(
                parserProgram->elementaryStreams.begin(), parserProgram->elementaryStreams.end(),
                [&binding](const auto& stream) { return stream.pid == binding.elementaryPid; });
            if (matches != 1) {
                return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                    ::media::ErrorInfo::invalidArgument("MPEG-TS public PID membership mismatch"));
            }
        }

        MediaTsSelectedProgramPlan candidate;
        candidate.programNumber = publicProgram.programNumber;
        candidate.programMapPid = publicProgram.pmtPid;
        candidate.pcrPid = publicProgram.pcrPid;
        const auto parserHas = [parserProgram](int pid) {
            return std::count_if(parserProgram->elementaryStreams.begin(), parserProgram->elementaryStreams.end(),
                [pid](const auto& stream) { return stream.pid == pid; }) == 1;
        };
        if (!parserHas(*videoPid) || !parserHas(*audioPid)) {
            return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS public PID is absent from parser program"));
        }
        candidate.videoPid = *videoPid;
        candidate.audioPid = *audioPid;
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
