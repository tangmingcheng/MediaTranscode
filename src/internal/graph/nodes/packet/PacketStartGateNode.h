#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class PacketStartGateNode final : public FFmpegNodeRuntime {
public:
    explicit PacketStartGateNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    void reset() noexcept;

private:
    bool m_configured = false;
    bool m_requireKeyFrame = false;
    bool m_open = false;
};

} // namespace media::ffmpeg::graph
