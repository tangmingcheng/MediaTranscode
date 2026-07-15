#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

#include <vector>

namespace media::ffmpeg::graph {

class MediaAvStartupReleaseBuffer;

class MediaAvBoundReleaseExtractorNode final : public FFmpegNodeRuntime {
public:
    explicit MediaAvBoundReleaseExtractorNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Result<bool> preflight(MediaGraphExecutionContext& context,
                                    const MediaAvStartupReleaseBuffer& release) const;
    ::media::Status commit(MediaGraphExecutionContext& context,
                           const MediaAvStartupReleaseBuffer& release);
    ::media::Status stageAudio(const MediaAvStartupReleaseBuffer& release);
    MediaBufferRef m_pending;
    std::vector<MediaBufferRef> m_stagedAudio;
};

} // namespace media::ffmpeg::graph
