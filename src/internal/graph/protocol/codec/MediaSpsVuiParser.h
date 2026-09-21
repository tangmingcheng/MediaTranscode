#pragma once
#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include "internal/graph/protocol/codec/MediaRbspBitReader.h"
#include <optional>

namespace media::ffmpeg::graph {
// Syntax and inference follow FFmpeg CBS cbs_h264/h265_syntax_template.c.
// A fact is published only after the complete supported SPS suffix and trailing bits.
class MediaSpsVuiParser final {
public:
    static std::optional<MediaVideoColorRangeFact> h264(MediaRbspBitReader& reader);
    static std::optional<MediaVideoColorRangeFact> hevc(
        MediaRbspBitReader& reader, std::uint32_t maximumSubLayersMinusOne);
};
} // namespace media::ffmpeg::graph
