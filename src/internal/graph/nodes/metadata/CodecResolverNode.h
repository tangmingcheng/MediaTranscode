#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVFormatContext;

namespace media::ffmpeg::graph {

class CodecResolverNode final : public FFmpegNodeRuntime {
public:
    explicit CodecResolverNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status resolveDecoder(MediaGraphExecutionContext& context, AVFormatContext* formatContext);
    ::media::Status resolveEncoder(MediaGraphExecutionContext& context, AVFormatContext* formatContext);

private:
    bool m_emitted = false;
    ::media::ffmpeg::BufferRefPtr m_decoderHardwareDevice;
    AVPixelFormat m_decoderHardwarePixelFormat = AV_PIX_FMT_NONE;
};

} // namespace media::ffmpeg::graph
