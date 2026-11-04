#pragma once
#include "internal/graph/diagnostics/MediaAccessUnitProcessingTrace.h"

#include "internal/graph/protocol/mpegts/MediaTsAccessUnitView.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>

namespace media::ffmpeg::graph {

class MediaTsAccessUnitBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef outer,
        MediaScheduledStream stream,
        std::uint64_t generation,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster,
        MediaRunningTime emitOnMaster,
        MediaRunningTime transportDecodeLead,
        MediaAccessUnitProcessingTrace processingTrace);

    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    ::media::Result<MediaTsAccessUnitView> view() const noexcept;
    const MediaAccessUnitProcessingTrace& processingTrace() const noexcept { return m_processingTrace; }

private:
    MediaTsAccessUnitBuffer(std::shared_ptr<FFmpegPacketBuffer> outer,
                            MediaScheduledStream stream,
                            std::uint64_t generation,
                            MediaRunningTime presentationOnMaster,
                            MediaRunningTime dispatchOnMaster,
                            MediaRunningTime emitOnMaster,
                            MediaAccessUnitProcessingTrace processingTrace);

    const std::shared_ptr<FFmpegPacketBuffer> m_outer;
    const MediaScheduledStream m_stream;
    const std::uint64_t m_generation;
    const MediaRunningTime m_presentationOnMaster;
    const MediaRunningTime m_dispatchOnMaster;
    const MediaRunningTime m_emitOnMaster;
    const bool m_randomAccess;
    const MediaAccessUnitProcessingTrace m_processingTrace;
};

} // namespace media::ffmpeg::graph
