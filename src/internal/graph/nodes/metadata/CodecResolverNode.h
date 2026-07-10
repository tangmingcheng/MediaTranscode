#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg::graph {

struct FFmpegInputStreamSnapshot;

class CodecResolverNode final : public FFmpegNodeRuntime {
public:
    explicit CodecResolverNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status resolveDecoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream);
    ::media::Status resolveEncoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream);

private:
    bool m_emitted = false;
    ::media::ffmpeg::BufferRefPtr m_decoderHardwareDevice;
    AVPixelFormat m_decoderHardwarePixelFormat = AV_PIX_FMT_NONE;
};

} // namespace media::ffmpeg::graph
