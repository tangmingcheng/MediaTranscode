#pragma once
#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/protocol/codec/MediaAnnexBAccessUnitValidator.h"
#include <span>
#include <vector>
namespace media::ffmpeg::graph {
class MediaVideoNalUnitScanner final {
public:
    using Visitor = void (*)(void*, std::span<const std::uint8_t>);
    static ::media::Status visit(std::span<const std::uint8_t> bytes, MediaAnnexBCodec codec,
        const MediaEncodedPacketLayout& layout, void* state, Visitor visitor);
    static ::media::Result<std::vector<std::span<const std::uint8_t>>> scan(
        std::span<const std::uint8_t> bytes, MediaAnnexBCodec codec,
        const MediaEncodedPacketLayout& layout);
};
} // namespace media::ffmpeg::graph
