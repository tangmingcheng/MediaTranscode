#include "internal/graph/protocol/mpegts/MediaTsProgramContractValidator.h"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <unordered_set>

namespace media::ffmpeg::graph {
namespace {

bool validPid(int pid) noexcept
{
    return pid > 0 && pid < 0x1FFF;
}

bool validStream(const MediaTsSelectedStreamPlan& stream) noexcept
{
    return stream.streamIndex >= 0 && validPid(stream.elementaryPid) &&
        MediaTsProgramContractValidator::isSelectedStreamTimeBase(
            stream.timeBase);
}

template <typename Selection>
bool validTypedSelection(const Selection& selection) noexcept
{
    const bool commonValid = selection.programNumber > 0 &&
        selection.programNumber <=
            (std::numeric_limits<std::uint16_t>::max)() &&
        validPid(selection.programMapPid) && validPid(selection.pcrPid) &&
        validStream(selection.video);
    if constexpr (std::is_same_v<
                      Selection,
                      MediaTsAudioVideoProgramSelection>) {
        return commonValid && validStream(selection.audio) &&
            selection.video.streamIndex != selection.audio.streamIndex &&
            selection.video.elementaryPid != selection.audio.elementaryPid;
    }
    return commonValid;
}

bool validDuration(
    const MediaTsPacketDurationEvidence& evidence,
    const MediaTsSelectedStreamPlan& stream) noexcept
{
    return evidence.streamIndex == stream.streamIndex &&
        evidence.elementaryPid == stream.elementaryPid &&
        evidence.packetDuration > 0 &&
        evidence.timeBase.num == stream.timeBase.num &&
        evidence.timeBase.den == stream.timeBase.den;
}

} // namespace

bool MediaTsProgramContractValidator::isSelectedStreamTimeBase(
    MediaRational timeBase) noexcept
{
    return timeBase.num == 1 && timeBase.den == 90'000;
}

::media::Status MediaTsProgramContractValidator::validateSnapshots(
    const std::vector<FFmpegInputProgramSnapshot>& publicPrograms,
    const MediaTsProgramInventorySnapshot& parserInventory)
{
    if (publicPrograms.empty() || parserInventory.programs.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MPEG-TS program snapshots are empty"));
    }
    std::unordered_set<int> publicProgramNumbers;
    for (const auto& publicProgram : publicPrograms) {
        if (!publicProgramNumbers.insert(publicProgram.programNumber).second ||
            publicProgram.programNumber <= 0 ||
            publicProgram.programNumber >
                (std::numeric_limits<std::uint16_t>::max)() ||
            !validPid(publicProgram.pmtPid) ||
            !validPid(publicProgram.pcrPid)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS public program identity is invalid or duplicated"));
        }
        const auto parserMatches = std::count_if(
            parserInventory.programs.begin(), parserInventory.programs.end(),
            [&publicProgram](const MediaTsProgramInfo& program) {
                return program.programNumber == publicProgram.programNumber;
            });
        if (parserMatches != 1) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS parser program identity is missing or duplicated"));
        }
        const auto parserProgram = std::find_if(
            parserInventory.programs.begin(), parserInventory.programs.end(),
            [&publicProgram](const MediaTsProgramInfo& program) {
                return program.programNumber == publicProgram.programNumber;
            });
        if (parserProgram->pmtPid != publicProgram.pmtPid ||
            parserProgram->pcrPid != publicProgram.pcrPid ||
            parserProgram->elementaryStreams.size() !=
                publicProgram.streamBindings.size()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS public program and parser inventory mismatch"));
        }
        std::unordered_set<int> streamIndexes;
        std::unordered_set<int> publicPids;
        for (const auto& binding : publicProgram.streamBindings) {
            if (binding.streamIndex < 0 ||
                !validPid(binding.elementaryPid) ||
                !streamIndexes.insert(binding.streamIndex).second ||
                !publicPids.insert(binding.elementaryPid).second) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS public stream binding is invalid or duplicated"));
            }
        }
        std::unordered_set<int> parserPids;
        for (const auto& stream : parserProgram->elementaryStreams) {
            if (!validPid(stream.pid) || !parserPids.insert(stream.pid).second) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS parser elementary PID is invalid or duplicated"));
            }
        }
        if (publicPids != parserPids) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS public and parser PID membership differs"));
        }
    }
    if (publicProgramNumbers.size() != parserInventory.programs.size()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS public and parser program membership differs"));
    }
    return ::media::Status::success();
}

::media::Status MediaTsProgramContractValidator::validateSelection(
    const MediaTsProgramSelection& selection)
{
    const bool valid = std::visit(
        [](const auto& selected) { return validTypedSelection(selected); },
        selection);
    return valid
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::invalidArgument(
              "invalid MPEG-TS typed program selection"));
}

::media::Status MediaTsProgramContractValidator::validateSelectedProgram(
    const MediaTsSelectedProgramPlan& selectedProgram)
{
    const bool valid = std::visit(
        [](const auto& program) {
            using Program = std::decay_t<decltype(program)>;
            if (!validTypedSelection(program.selection) ||
                !validDuration(
                    program.videoPacketDuration,
                    program.selection.video)) {
                return false;
            }
            if constexpr (std::is_same_v<
                              Program,
                              MediaTsAudioVideoSelectedProgramPlan>) {
                return validDuration(
                    program.audioPacketDuration,
                    program.selection.audio);
            }
            return true;
        },
        selectedProgram);
    return valid
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::invalidArgument(
              "invalid MPEG-TS selected program evidence contract"));
}

} // namespace media::ffmpeg::graph
