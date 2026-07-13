#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

struct FFmpegInputStreamSnapshot;
class FFmpegInputSnapshotBuffer;

class AudioCodecResolverNode final : public FFmpegNodeRuntime {
public:
    explicit AudioCodecResolverNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Result<const FFmpegInputStreamSnapshot*> resolveSourceStream(MediaGraphExecutionContext& context,
                                                                          const FFmpegInputSnapshotBuffer& format) const;
    ::media::Result<::media::ffmpeg::CodecContextPtr> buildDecoderContext(const FFmpegInputStreamSnapshot& stream) const;
    ::media::Result<::media::ffmpeg::CodecContextPtr> buildEncoderContext(MediaGraphExecutionContext& context,
                                                                           const FFmpegInputStreamSnapshot& stream,
                                                                           const AVCodecContext* decoderContext) const;
    ::media::Status emitCodecContext(MediaGraphExecutionContext& context,
                                      const char* portName,
                                      ::media::ffmpeg::CodecContextPtr codecContext);

private:
    bool m_emitted = false;
};

} // namespace media::ffmpeg::graph
