#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoEncoderCodecApi.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

#include <set>
#include <string_view>

namespace media::ffmpeg::graph {

class VideoEncodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit VideoEncodeNode(MediaNodeId nodeId);
    VideoEncodeNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry);
    VideoEncodeNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry,
        std::shared_ptr<MediaVideoEncoderCodecApi> codecApi);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status emitEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& codecBuffer);
    ::media::Result<bool> receivePackets(MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> submitPendingFrame(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> continueFlush(MediaGraphExecutionContext& context);
    ::media::Status drainEncoderForStop();
    void resetRuntimeState() noexcept;
    ::media::Status attachPendingLineage();

private:
    bool m_encoderConfigEmitted = false;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
    bool m_flushPending = false;
    bool m_receivePending = false;
    bool m_flushIsEof = false;
    bool m_flushSent = false;
    MediaBufferRef m_flushBuffer;
    ::media::ffmpeg::FramePtr m_pendingFrame;
    std::shared_ptr<const MediaCanonicalLineage> m_pendingLineage;
    std::shared_ptr<MediaCodecLineageRegistry> m_lineageRegistry;
    std::shared_ptr<MediaVideoEncoderCodecApi> m_codecApi;
    std::set<std::uint64_t> m_lineageGenerations;
};

} // namespace media::ffmpeg::graph
