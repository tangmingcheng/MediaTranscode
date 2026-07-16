#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoEncoderCodecApi.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaVideoLineageState.h"

#include <set>
#include <string_view>

namespace media::ffmpeg::graph {

class VideoEncodeLineageState final : public MediaVideoLineageState {
public:
    VideoEncodeLineageState(
        std::shared_ptr<MediaCodecLineageRegistry> registry,
        std::shared_ptr<MediaVideoEncoderCodecApi> codecApi) noexcept;

    MediaInputTerminalTracker terminals { { "frame" } };
    bool eofEmitted = false;
    bool receivePending = false;
    bool flushPending = false;
    bool flushIsEof = false;
    bool flushSent = false;
    MediaBufferRef flushBuffer;
    ::media::ffmpeg::FramePtr pendingFrame;
    std::shared_ptr<const MediaCanonicalLineage> pendingLineage;
    std::set<std::uint64_t> lineageGenerations;

    void bindCodec(MediaBufferRef owner, AVCodecContext* context) noexcept;
    void resetCodecBinding() noexcept;
    void resetForLifecycle() noexcept;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearLineageStorage() noexcept;
    std::shared_ptr<MediaVideoEncoderCodecApi> m_codecApi;
    MediaBufferRef m_codecOwner;
    AVCodecContext* m_codecContext = nullptr;
};

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
    std::shared_ptr<MediaCodecLineageRegistry> m_lineageRegistry;
    std::shared_ptr<MediaVideoEncoderCodecApi> m_codecApi;
    std::shared_ptr<VideoEncodeLineageState> m_lineageState;
};

} // namespace media::ffmpeg::graph
