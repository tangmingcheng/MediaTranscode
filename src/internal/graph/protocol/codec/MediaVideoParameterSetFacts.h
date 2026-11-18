#pragma once
#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include "internal/graph/protocol/codec/MediaVideoNalUnitScanner.h"
#include <optional>
struct AVCodecContext;

namespace media::ffmpeg::graph {
class MediaVideoParameterSetFacts final {
public:
    static std::optional<MediaVideoColorRangeFact> fromExtradata(const AVCodecContext& context);
    static std::optional<MediaVideoColorRangeFact> fromAccessUnit(
        std::span<const std::uint8_t> bytes, MediaAnnexBCodec codec,
        const MediaEncodedPacketLayout& layout);
};
} // namespace media::ffmpeg::graph
