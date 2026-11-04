#pragma once

#include "internal/graph/model/MediaDecoderInputRetention.h"
#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"

namespace media::ffmpeg::graph {

struct MediaVideoDecoderPreparationStoragePlan final {
    std::size_t packetBytes;
    std::size_t ownerBytes;
    std::size_t softwareFrameBytes;
};

class MediaVideoDecoderPreparationStoragePlanner final {
public:
    static ::media::Result<MediaVideoDecoderPreparationStoragePlan> plan(
        std::size_t maximumAccessUnitBytes,
        const MediaDecoderInputRetention& retention,
        const CodecResolverDecoderContextBuildResult& decoder,
        const MediaHardwareDescriptor& frameContract);
};

} // namespace media::ffmpeg::graph
