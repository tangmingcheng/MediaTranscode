#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/network/MediaDatagramServiceShaper.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaDatagramShaperNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<std::unique_ptr<MediaDatagramShaperNode>> create(
        MediaNodeId nodeId,
        std::shared_ptr<MediaMasterClock> clock);
    static MediaNodeKind staticKind() noexcept;

    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    MediaDatagramShaperNode(MediaNodeId nodeId,
                            std::shared_ptr<MediaMasterClock> clock) noexcept;
    ::media::Status validatePorts(MediaGraphExecutionContext& context) const;
    void emitDiagnostics(const char* stage) noexcept;

    std::shared_ptr<MediaMasterClock> m_clock;
    std::unique_ptr<MediaDatagramServiceShaper> m_shaper;
    bool m_diagnosticsEmitted = false;
};

} // namespace media::ffmpeg::graph
