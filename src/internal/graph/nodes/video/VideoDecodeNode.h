#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCodecApi.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaVideoLineageState.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"

#include <set>
#include <string_view>
#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

class MediaGraphPayloadCreditLease;

class VideoDecodeLineageState final : public MediaVideoLineageState {
public:
    VideoDecodeLineageState(
        std::shared_ptr<MediaCodecLineageRegistry> registry,
        std::shared_ptr<MediaVideoDecoderCodecApi> codecApi) noexcept;

    MediaInputTerminalTracker terminals { { "packet" } };
    bool eofEmitted = false;
    bool receivePending = false;
    bool flushPending = false;
    bool flushIsEof = false;
    bool flushSent = false;
    MediaBufferRef flushBuffer;
    ::media::ffmpeg::PacketPtr pendingPacket;
    std::shared_ptr<MediaGraphPayloadCreditLease> pendingPayloadCredit;
    std::shared_ptr<const MediaCanonicalLineage> pendingLineage;
    std::set<std::uint64_t> lineageGenerations;
    AVBufferRef* pendingSubmissionLineage = nullptr;
    std::deque<AVBufferRef*> submissionOrderLineage;

    void bindCodec(MediaBufferRef owner, AVCodecContext* context) noexcept;
    void resetCodecBinding() noexcept;
    void resetForLifecycle() noexcept;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearLineageStorage() noexcept;
    std::shared_ptr<MediaVideoDecoderCodecApi> m_codecApi;
    MediaBufferRef m_codecOwner;
    AVCodecContext* m_codecContext = nullptr;
};

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

    std::shared_ptr<MediaCodecLineageRegistry> m_lineageRegistry;
    std::shared_ptr<MediaVideoDecoderCodecApi> m_codecApi;
    std::shared_ptr<VideoDecodeLineageState> m_lineageState;
    bool m_firstPacketDiagnosticEmitted = false;
    bool m_firstSubmitDiagnosticEmitted = false;
    bool m_firstFrameDiagnosticEmitted = false;
    std::optional<MediaHardwareDescriptor> m_outputContract;
    std::optional<bool> m_copyOpaqueLineage;
    std::uint64_t m_drmPrimeFrames = 0;
    std::uint64_t m_softwareFrames = 0;
};

} // namespace media::ffmpeg::graph
