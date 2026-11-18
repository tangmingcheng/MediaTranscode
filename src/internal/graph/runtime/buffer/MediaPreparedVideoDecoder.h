#pragma once

#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshot.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"

#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

// One preparation transaction may publish its decoder to exactly one source.
// Claim does not open, reset, or choose a decoder. The caller must have released
// its probe frames and flushed the decoder before creating this product.
class MediaPreparedVideoDecoder final {
public:
    ::media::Result<CodecResolverDecoderContextBuildResult> claim(
        const FFmpegInputStreamSnapshot& source, const MediaNodeOptions& options);
    ::media::Status validateHardwareDevice(const AVBufferRef* device) const;

private:
    friend class MediaRtpSourceFirstFrameProbe;
    static ::media::Result<std::shared_ptr<MediaPreparedVideoDecoder>> create(
        CodecResolverDecoderContextBuildResult decoder,
        MediaPreparationStorageLease storage,
        const FFmpegInputStreamSnapshot& source, MediaHardwareDescriptor frameContract);

    MediaPreparedVideoDecoder(CodecResolverDecoderContextBuildResult decoder,
        ::media::ffmpeg::CodecParametersPtr parameters,
        int streamIndex, MediaTimeDescriptor time, MediaHardwareDescriptor frameContract);

    mutable std::mutex m_mutex;
    std::optional<CodecResolverDecoderContextBuildResult> m_decoder;
    ::media::ffmpeg::CodecParametersPtr m_parameters;
    int m_streamIndex;
    MediaTimeDescriptor m_time;
    MediaHardwareDescriptor m_frameContract;
};

} // namespace media::ffmpeg::graph
