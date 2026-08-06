#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketOriginPolicy.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramSelection.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"
#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsRuntimeStreamFacts final {
    int streamIndex;
    MediaStreamKind streamKind;
    MediaRational timeBase;
};

struct MediaTsRuntimeStreamBinding final {
    MediaTsRuntimeStreamBinding() = delete;
    MediaTsRuntimeStreamBinding(
        int selectedStreamIndex,
        std::uint16_t selectedPid,
        MediaRational selectedTimeBase) noexcept
        : streamIndex(selectedStreamIndex),
          pid(selectedPid),
          timeBase(selectedTimeBase)
    {
    }

    int streamIndex;
    std::uint16_t pid;
    MediaRational timeBase;
    friend bool operator==(const MediaTsRuntimeStreamBinding& left,
                           const MediaTsRuntimeStreamBinding& right) noexcept
    {
        return left.streamIndex == right.streamIndex &&
            left.pid == right.pid &&
            left.timeBase.num == right.timeBase.num &&
            left.timeBase.den == right.timeBase.den;
    }
};

struct MediaTsVideoOnlyRuntimeBinding final {
    MediaTsVideoOnlyRuntimeBinding() = delete;
    MediaTsVideoOnlyRuntimeBinding(
        int selectedProgramNumber,
        int selectedProgramMapPid,
        MediaTsPacketOriginPolicy selectedOriginPolicy,
        MediaTsRuntimeStreamBinding selectedVideo,
        std::uint16_t selectedPcrPid,
        std::size_t selectedVideoPesProvenanceCapacity) noexcept
        : programNumber(selectedProgramNumber),
          programMapPid(selectedProgramMapPid),
          originPolicy(selectedOriginPolicy),
          video(selectedVideo),
          pcrPid(selectedPcrPid),
          videoPesProvenanceCapacity(selectedVideoPesProvenanceCapacity)
    {
    }

    int programNumber;
    int programMapPid;
    MediaTsPacketOriginPolicy originPolicy;
    MediaTsRuntimeStreamBinding video;
    std::uint16_t pcrPid;
    std::size_t videoPesProvenanceCapacity;
    bool operator==(const MediaTsVideoOnlyRuntimeBinding&) const = default;
};

struct MediaTsAudioVideoRuntimeBinding final {
    MediaTsAudioVideoRuntimeBinding() = delete;
    MediaTsAudioVideoRuntimeBinding(
        int selectedProgramNumber,
        int selectedProgramMapPid,
        MediaTsPacketOriginPolicy selectedOriginPolicy,
        MediaTsRuntimeStreamBinding selectedVideo,
        MediaTsRuntimeStreamBinding selectedAudio,
        std::uint16_t selectedPcrPid,
        std::size_t selectedPesProvenanceCapacity) noexcept
        : programNumber(selectedProgramNumber),
          programMapPid(selectedProgramMapPid),
          originPolicy(selectedOriginPolicy),
          video(selectedVideo),
          audio(selectedAudio),
          pcrPid(selectedPcrPid),
          pesProvenanceCapacity(selectedPesProvenanceCapacity)
    {
    }

    int programNumber;
    int programMapPid;
    MediaTsPacketOriginPolicy originPolicy;
    MediaTsRuntimeStreamBinding video;
    MediaTsRuntimeStreamBinding audio;
    std::uint16_t pcrPid;
    std::size_t pesProvenanceCapacity;
    bool operator==(const MediaTsAudioVideoRuntimeBinding&) const = default;
};

using MediaTsRuntimeBinding = std::variant<
    MediaTsVideoOnlyRuntimeBinding,
    MediaTsAudioVideoRuntimeBinding>;

class MediaTsRuntimeBindingCodec final {
public:
    static ::media::Result<MediaTsRuntimeBinding> create(
        const MediaTsProgramSelection& selection,
        MediaTsPacketOriginPolicy originPolicy,
        std::size_t pesProvenanceCapacity);
    static ::media::Status validate(
        const MediaTsRuntimeBinding& binding,
        std::size_t expectedPesProvenanceCapacity);
    static ::media::Result<MediaTsRuntimeBinding> rebindStreamIndexes(
        const MediaTsRuntimeBinding& binding,
        const std::vector<FFmpegInputProgramSnapshot>& programs,
        const MediaTsProgramInventorySnapshot& parserInventory,
        const std::vector<MediaTsRuntimeStreamFacts>& streams);
    static std::optional<MediaStreamKind> streamKindForIndex(
        const MediaTsRuntimeBinding& binding,
        int streamIndex) noexcept;
    static std::optional<MediaRational> timeBaseForIndex(
        const MediaTsRuntimeBinding& binding,
        int streamIndex) noexcept;
    static bool requiresSelectedPesBoundary(
        const MediaTsRuntimeBinding& binding,
        std::uint16_t pid) noexcept;
    static bool containsStreamIndex(
        const MediaTsRuntimeBinding& binding,
        int streamIndex) noexcept;
    static std::vector<std::uint16_t> selectedElementaryPids(
        const MediaTsRuntimeBinding& binding);
    static std::vector<std::uint16_t> sourceClockPids(
        const MediaTsRuntimeBinding& binding);

private:
    MediaTsRuntimeBindingCodec() = delete;
};

} // namespace media::ffmpeg::graph
