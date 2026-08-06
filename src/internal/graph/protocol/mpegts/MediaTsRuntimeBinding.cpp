#include "internal/graph/protocol/mpegts/MediaTsRuntimeBinding.h"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool validTimeBase(MediaRational timeBase) noexcept
{
    return timeBase.num > 0 && timeBase.den > 0;
}

bool validStream(const MediaTsRuntimeStreamBinding& stream) noexcept
{
    return stream.streamIndex >= 0 && stream.pid > 0 &&
        stream.pid < 0x1FFF && validTimeBase(stream.timeBase);
}

bool validSelectedStream(const MediaTsSelectedStreamPlan& stream) noexcept
{
    return stream.streamIndex >= 0 && stream.elementaryPid > 0 &&
        stream.elementaryPid < 0x1FFF && validTimeBase(stream.timeBase);
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
    std::size_t pesProvenanceCapacity)
{
    const bool selectionValid = std::visit(
        [](const auto& selected) {
            using Selection = std::decay_t<decltype(selected)>;
            const bool commonValid = selected.programNumber > 0 &&
                selected.programMapPid > 0 &&
                selected.programMapPid < 0x1FFF && selected.pcrPid > 0 &&
                selected.pcrPid < 0x1FFF &&
                validSelectedStream(selected.video);
            if constexpr (std::is_same_v<
                              Selection,
                              MediaTsAudioVideoProgramSelection>) {
                return commonValid && validSelectedStream(selected.audio) &&
                    selected.video.streamIndex != selected.audio.streamIndex &&
                    selected.video.elementaryPid !=
                        selected.audio.elementaryPid;
            }
            return commonValid;
        },
        selection);
    if (!selectionValid) {
        return ::media::Result<MediaTsRuntimeBinding>::failure(
            ::media::ErrorInfo::invalidArgument(
                "invalid MPEG-TS program selection for runtime binding"));
    }
    MediaTsRuntimeBinding binding = std::visit(
        [originPolicy, pesProvenanceCapacity](const auto& selected)
            -> MediaTsRuntimeBinding {
            using Selection = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Selection,
                              MediaTsVideoOnlyProgramSelection>) {
                return MediaTsVideoOnlyRuntimeBinding{
                    selected.programNumber,
                    selected.programMapPid,
                    originPolicy,
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
                selected.programMapPid > 0 && selected.programMapPid < 0x1FFF &&
                selected.originPolicy ==
                    MediaTsPacketOriginPolicy::PerStreamPesCarry &&
                validStream(selected.video) && selected.pcrPid > 0 &&
                selected.pcrPid < 0x1FFF;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsVideoOnlyRuntimeBinding>) {
                if (!commonValid || selected.videoPesProvenanceCapacity == 0 ||
                    selected.videoPesProvenanceCapacity !=
                        expectedPesProvenanceCapacity) {
                    return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "invalid VideoOnly MPEG-TS runtime binding"));
                }
            } else {
                if (!commonValid || !validStream(selected.audio) ||
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
    const FFmpegInputProgramSnapshot& program,
    const std::vector<MediaTsRuntimeStreamFacts>& streams)
{
    if (!matchesProgram(binding, program)) {
        return ::media::Result<MediaTsRuntimeBinding>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime program identity differs from planner preflight"));
    }
    return std::visit(
        [&program, &streams](const auto& selected)
            -> ::media::Result<MediaTsRuntimeBinding> {
            auto rebound = selected;
            const auto video = streamIndexForPid(program, selected.video.pid);
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
                const auto audio = streamIndexForPid(program, selected.audio.pid);
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

bool MediaTsRuntimeBindingCodec::matchesProgram(
    const MediaTsRuntimeBinding& binding,
    const FFmpegInputProgramSnapshot& program) noexcept
{
    return std::visit(
        [&program](const auto& selected) {
            return commonProgramMatches(selected, program);
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

} // namespace media::ffmpeg::graph
