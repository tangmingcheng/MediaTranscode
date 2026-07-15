#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCodecApi.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

#include <set>
#include <string_view>

namespace media::ffmpeg::graph {

class VideoDecodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit VideoDecodeNode(MediaNodeId nodeId);
    VideoDecodeNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry);
    VideoDecodeNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry,
        std::shared_ptr<MediaVideoDecoderCodecApi> codecApi);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;

private:
    friend struct VideoDecodeNodeLifecycleTestAccess;

    ::media::Result<bool> receiveFrames(MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> submitPendingPacket(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> continueFlush(MediaGraphExecutionContext& context);
    void resetRuntimeState() noexcept;
    ::media::Status attachPendingLineage();

    MediaInputTerminalTracker m_terminals { { "packet" } };
    bool m_eofEmitted = false;
    bool m_receivePending = false;
    bool m_flushPending = false;
    bool m_flushIsEof = false;
    bool m_flushSent = false;
    MediaBufferRef m_flushBuffer;
    ::media::ffmpeg::PacketPtr m_pendingPacket;
    std::shared_ptr<const MediaCanonicalLineage> m_pendingLineage;
    std::shared_ptr<MediaCodecLineageRegistry> m_lineageRegistry;
    std::shared_ptr<MediaVideoDecoderCodecApi> m_codecApi;
    std::set<std::uint64_t> m_lineageGenerations;
};

} // namespace media::ffmpeg::graph
