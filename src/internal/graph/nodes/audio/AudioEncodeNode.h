#pragma once

#include "internal/graph/nodes/audio/AudioEncoderCodecApi.h"
#include "internal/graph/nodes/audio/AudioEncoderFrameQueue.h"
#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageState.h"

#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

class AudioEncodeLineageState final : public MediaAudioLineageState {
public:
    AudioEncodeLineageState(MediaAudioLineageExecutionMode mode,
                            std::size_t capacity) noexcept;

    bool receivePending = false;
    AudioEncoderFrameQueue frameQueue;
    ::media::ffmpeg::FramePtr pendingFrame;
    std::vector<MediaAudioIntervalFragment> pendingFragments;
    std::deque<std::vector<MediaAudioIntervalFragment>> submittedFragments;
    std::optional<MediaAudioPlaybackOrigin> activeOrigin;
    bool flushPending = false;
    bool flushIsEof = false;
    bool flushSent = false;
    MediaBufferRef flushBuffer;
    MediaInputTerminalTracker terminals { { "frame" } };
    bool eofEmitted = false;

    void setCodecApi(std::shared_ptr<AudioEncoderCodecApi> codecApi) noexcept;
    void bindCodec(MediaBufferRef owner, AVCodecContext* context) noexcept;
    void resetCodecBinding() noexcept;
    void resetForLifecycle() noexcept;
    ::media::Status preflightIncomingLineage(
        const std::vector<MediaAudioIntervalFragment>& incoming) const;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearLineageStorage(bool resetFrameQueue) noexcept;
    std::shared_ptr<AudioEncoderCodecApi> m_codecApi;
    MediaBufferRef m_codecOwner;
    AVCodecContext* m_codecContext = nullptr;
};

class AudioEncodeNode final : public FFmpegCodecNodeRuntime {
public:
    using LineageState = AudioEncodeLineageState;

    AudioEncodeNode(MediaNodeId nodeId, MediaAudioLineageExecutionMode lineageMode,
                    std::shared_ptr<AudioEncodeLineageState> lineageState);
    AudioEncodeNode(MediaNodeId nodeId, MediaAudioLineageExecutionMode lineageMode,
                    std::shared_ptr<AudioEncodeLineageState> lineageState,
                    std::shared_ptr<AudioEncoderCodecApi> codecApi);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;

private:
    ::media::Status emitEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Result<bool> receivePackets(MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> encodeQueuedFrame(MediaGraphExecutionContext& context,
                                                              bool allowPartial);
    ::media::Result<MediaNodeProcessResult> continueFlush(MediaGraphExecutionContext& context);
    void resetRuntimeState() noexcept;

private:
    bool m_encoderConfigEmitted = false;
    bool m_queuedFrameBeforeCodecTraced = false;
    bool m_codecBoundTraced = false;
    bool m_firstFrameTraced = false;
    bool m_firstPacketTraced = false;
    std::shared_ptr<AudioEncodeLineageState> m_lineageState;
    bool& m_receivePending;
    bool& m_flushPending;
    bool& m_flushIsEof;
    bool& m_flushSent;
    MediaBufferRef& m_flushBuffer;
    std::shared_ptr<AudioEncoderCodecApi> m_codecApi;
    AudioEncoderFrameQueue& m_frameQueue;
    ::media::ffmpeg::FramePtr& m_pendingFrame;
    std::vector<MediaAudioIntervalFragment>& m_pendingFragments;
    std::deque<std::vector<MediaAudioIntervalFragment>>& m_submittedFragments;
    MediaAudioLineageExecutionMode m_lineageMode;
    std::optional<MediaAudioPlaybackOrigin>& m_activeOrigin;
};

} // namespace media::ffmpeg::graph
