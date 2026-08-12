#include "internal/graph/protocol/mpegts/MediaTsRuntimeBinding.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramContractValidator.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool validStream(const MediaTsRuntimeStreamBinding& stream) noexcept
{
    return stream.streamIndex >= 0 && stream.pid > 0 &&
        stream.pid < 0x1FFF &&
        MediaTsProgramContractValidator::isSelectedStreamTimeBase(
            stream.timeBase);
}

bool sameTimeBase(MediaRational left, MediaRational right) noexcept
{
    return left.num == right.num && left.den == right.den;
}

const MediaTsRuntimeStreamFacts* runtimeStream(
    const std::vector<MediaTsRuntimeStreamFacts>& streams,
    int streamIndex) noexcept
{
    const auto found = std::find_if(
        streams.begin(), streams.end(),
        [streamIndex](const MediaTsRuntimeStreamFacts& stream) {
            return stream.streamIndex == streamIndex;
        });
    return found == streams.end() ? nullptr : &*found;
}

std::optional<int> streamIndexForPid(
    const FFmpegInputProgramSnapshot& program,
    std::uint16_t pid) noexcept
{
    const auto found = std::find_if(
        program.streamBindings.begin(), program.streamBindings.end(),
        [pid](const FFmpegInputProgramStreamBinding& stream) {
            return stream.elementaryPid == pid;
        });
    if (found == program.streamBindings.end()) return std::nullopt;
    return found->streamIndex;
}

template <typename Binding>
bool commonProgramMatches(
    const Binding& binding,
    const FFmpegInputProgramSnapshot& program) noexcept
{
    return binding.programNumber == program.programNumber &&
        binding.programMapPid == program.pmtPid &&
        binding.pcrPid == program.pcrPid;
}

} // namespace

::media::Result<MediaTsRuntimeBinding> MediaTsRuntimeBindingCodec::create(
    const MediaTsProgramSelection& selection,
    MediaTsPacketOriginPolicy originPolicy,
    MediaTsUnexpectedElementaryPidPolicy unexpectedPidPolicy,
    std::size_t pesProvenanceCapacity)
{
    if (auto selected = MediaTsProgramContractValidator::validateSelection(
            selection);
        !selected) {
        return ::media::Result<MediaTsRuntimeBinding>::failure(
            selected.error());
    }
    MediaTsRuntimeBinding binding = std::visit(
        [originPolicy, unexpectedPidPolicy, pesProvenanceCapacity](const auto& selected)
            -> MediaTsRuntimeBinding {
            using Selection = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Selection,
                              MediaTsVideoOnlyProgramSelection>) {
                return MediaTsVideoOnlyRuntimeBinding{
                    selected.programNumber,
                    selected.programMapPid,
                    originPolicy,
                    unexpectedPidPolicy,
                    MediaTsRuntimeStreamBinding{
                        selected.video.streamIndex,
                        static_cast<std::uint16_t>(
                            selected.video.elementaryPid),
                        selected.video.timeBase},
                    static_cast<std::uint16_t>(selected.pcrPid),
                    pesProvenanceCapacity};
            } else {
                return MediaTsAudioVideoRuntimeBinding{
                    selected.programNumber,
                    selected.programMapPid,
                    originPolicy,
                    unexpectedPidPolicy,
                    MediaTsRuntimeStreamBinding{
                        selected.video.streamIndex,
                        static_cast<std::uint16_t>(
                            selected.video.elementaryPid),
                        selected.video.timeBase},
                    MediaTsRuntimeStreamBinding{
                        selected.audio.streamIndex,
                        static_cast<std::uint16_t>(
                            selected.audio.elementaryPid),
                        selected.audio.timeBase},
                    static_cast<std::uint16_t>(selected.pcrPid),
                    pesProvenanceCapacity};
            }
        },
        selection);
    if (auto status = validate(binding, pesProvenanceCapacity); !status) {
        return ::media::Result<MediaTsRuntimeBinding>::failure(status.error());
    }
    return ::media::Result<MediaTsRuntimeBinding>::success(std::move(binding));
}

::media::Status MediaTsRuntimeBindingCodec::validate(
    const MediaTsRuntimeBinding& binding,
    std::size_t expectedPesProvenanceCapacity)
{
    return std::visit(
        [expectedPesProvenanceCapacity](const auto& selected)
            -> ::media::Status {
            using Binding = std::decay_t<decltype(selected)>;
            const bool commonValid = selected.programNumber > 0 &&
                selected.programNumber <=
                    (std::numeric_limits<std::uint16_t>::max)() &&
                selected.programMapPid > 0 && selected.programMapPid < 0x1FFF &&
                selected.originPolicy ==
                    MediaTsPacketOriginPolicy::PerStreamPesCarry &&
                validStream(selected.video) && selected.pcrPid > 0 &&
                selected.pcrPid < 0x1FFF;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsVideoOnlyRuntimeBinding>) {
                if (!commonValid ||
                    selected.unexpectedPidPolicy !=
                        MediaTsUnexpectedElementaryPidPolicy::Ignore ||
                    selected.videoPesProvenanceCapacity == 0 ||
                    selected.videoPesProvenanceCapacity !=
                        expectedPesProvenanceCapacity) {
                    return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "invalid VideoOnly MPEG-TS runtime binding"));
                }
            } else {
                if (!commonValid ||
                    selected.unexpectedPidPolicy !=
                        MediaTsUnexpectedElementaryPidPolicy::Reject ||
                    !validStream(selected.audio) ||
                    selected.video.streamIndex == selected.audio.streamIndex ||
                    selected.video.pid == selected.audio.pid ||
                    selected.pesProvenanceCapacity == 0 ||
                    selected.pesProvenanceCapacity !=
                        expectedPesProvenanceCapacity) {
                    return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "invalid AudioVideo MPEG-TS runtime binding"));
                }
            }
            return ::media::Status::success();
        },
        binding);
}

::media::Result<MediaTsRuntimeBinding>
MediaTsRuntimeBindingCodec::rebindStreamIndexes(
    const MediaTsRuntimeBinding& binding,
    const std::vector<FFmpegInputProgramSnapshot>& programs,
    const MediaTsProgramInventorySnapshot& parserInventory,
    const std::vector<MediaTsRuntimeStreamFacts>& streams)
{
    if (auto snapshots = MediaTsProgramContractValidator::validateSnapshots(
            programs, parserInventory);
        !snapshots) {
        return ::media::Result<MediaTsRuntimeBinding>::failure(
            snapshots.error());
    }
    std::unordered_set<int> runtimeIndexes;
    for (const auto& stream : streams) {
        if (stream.streamIndex < 0 ||
            !runtimeIndexes.insert(stream.streamIndex).second ||
            !MediaTsProgramContractValidator::isSelectedStreamTimeBase(
                stream.timeBase)) {
            return ::media::Result<MediaTsRuntimeBinding>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS runtime stream facts are invalid or duplicated"));
        }
    }
    const auto matches = std::count_if(
        programs.begin(), programs.end(), [&binding](const auto& program) {
            return std::visit(
                [&program](const auto& selected) {
                    return commonProgramMatches(selected, program);
                },
                binding);
        });
    if (matches != 1) {
        return ::media::Result<MediaTsRuntimeBinding>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime program identity is missing or ambiguous"));
    }
    const auto program = std::find_if(
        programs.begin(), programs.end(), [&binding](const auto& candidate) {
            return std::visit(
                [&candidate](const auto& selected) {
                    return commonProgramMatches(selected, candidate);
                },
                binding);
        });
    return std::visit(
        [program, &streams](const auto& selected)
            -> ::media::Result<MediaTsRuntimeBinding> {
            auto rebound = selected;
            const auto video = streamIndexForPid(*program, selected.video.pid);
            if (!video) {
                return ::media::Result<MediaTsRuntimeBinding>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS runtime video identity differs from planner preflight"));
            }
            rebound.video.streamIndex = *video;
            const auto* runtimeVideo = runtimeStream(streams, *video);
            if (!runtimeVideo ||
                runtimeVideo->streamKind != MediaStreamKind::Video ||
                !sameTimeBase(
                    runtimeVideo->timeBase, selected.video.timeBase)) {
                return ::media::Result<MediaTsRuntimeBinding>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS runtime video facts differ from planner preflight"));
            }
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsAudioVideoRuntimeBinding>) {
                const auto audio = streamIndexForPid(*program, selected.audio.pid);
                if (!audio || *audio == *video) {
                    return ::media::Result<MediaTsRuntimeBinding>::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "MPEG-TS runtime audio identity differs from planner preflight"));
                }
                rebound.audio.streamIndex = *audio;
                const auto* runtimeAudio = runtimeStream(streams, *audio);
                if (!runtimeAudio ||
                    runtimeAudio->streamKind != MediaStreamKind::Audio ||
                    !sameTimeBase(
                        runtimeAudio->timeBase,
                        selected.audio.timeBase)) {
                    return ::media::Result<MediaTsRuntimeBinding>::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "MPEG-TS runtime audio facts differ from planner preflight"));
                }
            }
            return ::media::Result<MediaTsRuntimeBinding>::success(
                MediaTsRuntimeBinding{std::move(rebound)});
        },
        binding);
}

std::optional<MediaStreamKind>
MediaTsRuntimeBindingCodec::streamKindForIndex(
    const MediaTsRuntimeBinding& binding,
    int streamIndex) noexcept
{
    return std::visit(
        [streamIndex](const auto& selected) -> std::optional<MediaStreamKind> {
            if (streamIndex == selected.video.streamIndex) {
                return MediaStreamKind::Video;
            }
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsAudioVideoRuntimeBinding>) {
                if (streamIndex == selected.audio.streamIndex) {
                    return MediaStreamKind::Audio;
                }
            }
            return std::nullopt;
        },
        binding);
}

std::optional<MediaRational> MediaTsRuntimeBindingCodec::timeBaseForIndex(
    const MediaTsRuntimeBinding& binding,
    int streamIndex) noexcept
{
    return std::visit(
        [streamIndex](const auto& selected) -> std::optional<MediaRational> {
            if (streamIndex == selected.video.streamIndex) {
                return selected.video.timeBase;
            }
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsAudioVideoRuntimeBinding>) {
                if (streamIndex == selected.audio.streamIndex) {
                    return selected.audio.timeBase;
                }
            }
            return std::nullopt;
        },
        binding);
}

bool MediaTsRuntimeBindingCodec::requiresSelectedPesBoundary(
    const MediaTsRuntimeBinding& binding,
    std::uint16_t pid) noexcept
{
    return std::visit(
        [pid](const auto& selected) {
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsVideoOnlyRuntimeBinding>) {
                return pid == selected.video.pid || pid == selected.pcrPid;
            } else {
                return pid == selected.video.pid || pid == selected.audio.pid ||
                    pid == selected.pcrPid;
            }
        },
        binding);
}

bool MediaTsRuntimeBindingCodec::containsStreamIndex(
    const MediaTsRuntimeBinding& binding,
    int streamIndex) noexcept
{
    return std::visit(
        [streamIndex](const auto& selected) {
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsVideoOnlyRuntimeBinding>) {
                return streamIndex == selected.video.streamIndex;
            } else {
                return streamIndex == selected.video.streamIndex ||
                    streamIndex == selected.audio.streamIndex;
            }
        },
        binding);
}

std::vector<std::uint16_t>
MediaTsRuntimeBindingCodec::selectedElementaryPids(
    const MediaTsRuntimeBinding& binding)
{
    return std::visit(
        [](const auto& selected) {
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsVideoOnlyRuntimeBinding>) {
                return std::vector<std::uint16_t>{selected.video.pid};
            } else {
                return std::vector<std::uint16_t>{
                    selected.video.pid, selected.audio.pid};
            }
        },
        binding);
}

std::vector<std::uint16_t> MediaTsRuntimeBindingCodec::sourceClockPids(
    const MediaTsRuntimeBinding& binding)
{
    auto result = selectedElementaryPids(binding);
    const auto pcrPid = std::visit(
        [](const auto& selected) { return selected.pcrPid; }, binding);
    if (std::find(result.begin(), result.end(), pcrPid) == result.end()) {
        result.push_back(pcrPid);
    }
    return result;
}

MediaTsUnexpectedElementaryPidPolicy
MediaTsRuntimeBindingCodec::unexpectedElementaryPidPolicy(
    const MediaTsRuntimeBinding& binding) noexcept
{
    return std::visit(
        [](const auto& selected) {
            return selected.unexpectedPidPolicy;
        },
        binding);
}

} // namespace media::ffmpeg::graph
