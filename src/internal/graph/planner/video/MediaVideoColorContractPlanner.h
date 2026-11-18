#pragma once
#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include "media_transcode/Result.h"
#include <span>
struct AVFrame;
namespace media::ffmpeg::graph {
class MediaVideoColorContractPlanner final {
public:
    static ::media::Result<MediaVideoColorContract> plan(
        std::span<const AVFrame* const> decoded, std::span<const AVFrame* const> tiles,
        std::span<const MediaVideoColorRangeFact> signaling);
    static ::media::Status validate(const MediaVideoColorContract& color);
};
} // namespace media::ffmpeg::graph
