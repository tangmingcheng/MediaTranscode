#pragma once

#include "internal/graph/nodes/audio/AudioDecoderCodecApi.h"
#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageState.h"
#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "internal/graph/sync/lineage/MediaAudioSampleProjection.h"

#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

class AudioDecodeLineageState final : public MediaAudioLineageState {
public:
    AudioDecodeLineageState(MediaAudioLineageExecutionMode mode,
                            std::size_t capacity) noexcept;

    bool receivePending = false;
    ::media::ffmpeg::PacketPtr pendingPacket;
    MediaAudioIntervalAccumulator intervals;
    std::optional<MediaAudioSampleProjection> intervalProjection;
    std::optional<MediaAudioPlaybackOrigin> activeOrigin;
    std::uint32_t startupTrimDirective = 0;
    bool startupTrimDirectiveEmitted = false;
    bool flushPending = false;
    bool flushIsEof = false;
    bool flushSent = false;
    MediaBufferRef flushBuffer;
    MediaInputTerminalTracker terminals { { "packet" } };
    bool eofEmitted = false;

    void setCodecApi(std::shared_ptr<AudioDecoderCodecApi> codecApi) noexcept;
    void bindCodec(MediaBufferRef owner, AVCodecContext* context) noexcept;
    void resetCodecBinding() noexcept;
    void resetForLifecycle() noexcept;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearLineageStorage() noexcept;
    std::shared_ptr<AudioDecoderCodecApi> m_codecApi;
    MediaBufferRef m_codecOwner;
    AVCodecContext* m_codecContext = nullptr;
};

class AudioDecodeNode final : public FFmpegCodecNodeRuntime {
public:
    using LineageState = AudioDecodeLineageState;

    AudioDecodeNode(MediaNodeId nodeId, MediaAudioLineageExecutionMode lineageMode,
                    std::shared_ptr<AudioDecodeLineageState> lineageState);
    AudioDecodeNode(MediaNodeId nodeId, MediaAudioLineageExecutionMode lineageMode,
                    std::shared_ptr<AudioDecodeLineageState> lineageState,
                    std::shared_ptr<AudioDecoderCodecApi> codecApi);
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
    ::media::Result<bool> receiveFrames(MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> submitPendingPacket(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> continueFlush(MediaGraphExecutionContext& context);
    void resetRuntimeState() noexcept;

    MediaAudioLineageExecutionMode m_lineageMode;
    std::shared_ptr<AudioDecodeLineageState> m_lineageState;
    std::shared_ptr<AudioDecoderCodecApi> m_codecApi;
    bool& m_flushPending;
    bool& m_flushIsEof;
    bool& m_flushSent;
    MediaBufferRef& m_flushBuffer;
};

} // namespace media::ffmpeg::graph
