#pragma once

#include "internal/graph/model/MediaEncoderOpenContract.h"
#include "media_transcode/Result.h"

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaEncoderOpenContractAdapter final {
public:
    static void applyLowLatency(AVCodecContext& context, bool lowLatency) noexcept;
    static ::media::Status applyBeforeOpen(
        AVCodecContext& context,
        const MediaEncoderOpenContract& contract);

    static ::media::Status validateEquivalentReadback(
        const AVCodecContext& productionProbe,
        const AVCodecContext& packetLayoutProbe,
        const MediaEncoderOpenContract& contract);

    MediaEncoderOpenContractAdapter() = delete;
};

} // namespace media::ffmpeg::graph
