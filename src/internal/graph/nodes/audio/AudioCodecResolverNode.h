#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg::graph {

class AudioCodecResolverNode final : public FFmpegNodeRuntime {
public:
    explicit AudioCodecResolverNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Result<int> resolveSourceStreamIndex(MediaGraphExecutionContext& context, AVFormatContext* formatContext) const;
    ::media::Result<::media::ffmpeg::CodecContextPtr> buildDecoderContext(AVStream* stream) const;
    ::media::Result<::media::ffmpeg::CodecContextPtr> buildEncoderContext(MediaGraphExecutionContext& context,
                                                                           const AVStream* stream,
                                                                           const AVCodecContext* decoderContext) const;
    ::media::Status emitCodecContext(MediaGraphExecutionContext& context,
                                      const char* portName,
                                      ::media::ffmpeg::CodecContextPtr codecContext);

private:
    bool m_emitted = false;
};

} // namespace media::ffmpeg::graph
