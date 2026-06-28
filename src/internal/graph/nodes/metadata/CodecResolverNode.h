#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

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
};

} // namespace media::ffmpeg::graph
