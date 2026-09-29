#pragma once

#include "internal/graph/model/MediaDecoderInputRetention.h"
#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpAccessUnitEnvelope.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"

namespace media::ffmpeg::graph {

// Logical retained payload and public object headers, not allocator RSS or
// opaque FFmpeg/device/driver allocations. Raw capture/snapshot has its own cap.
struct MediaSourcePreparationStoragePlan final {
    std::size_t maximumBytes;
    std::size_t maximumDatagramBytes;
    std::size_t maximumReorderedPackets;
    std::size_t maximumAccessUnitBytes;
    std::size_t maximumAccessUnitsPerPush;
    std::size_t parserReorderBytes;
    std::size_t signalingBytes;
    std::size_t assemblyBytes;
    std::size_t decoderPacketBytes;
    std::size_t sourceOwnerBytes;
    std::size_t softwareFrameBytes;
    std::size_t configurationBytes;
    std::string authority;
};

class MediaSourcePreparationStoragePlanner final {
public:
    static ::media::Result<MediaSourcePreparationStoragePlan> plan(
        std::size_t maximumDatagramBytes,
        const MediaRtpReorderConfig& reorder,
        const MediaRtpDepacketizerConfig& depacketizer,
        const MediaPreparedRtpAccessUnitEnvelope& envelope,
        const MediaDecoderInputRetention& retention,
        const CodecResolverDecoderContextBuildResult& decoder,
        const MediaHardwareDescriptor& frameContract);
};

} // namespace media::ffmpeg::graph
