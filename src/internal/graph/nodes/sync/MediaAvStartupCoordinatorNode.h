#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"

#include <memory>
#include <deque>
#include <optional>
#include <string_view>
#include <unordered_map>
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

class MediaAvStartupEnvelopeBuffer;
class MediaControlBuffer;
class MediaAvStartupGenerationState;
class MediaAvGenerationPurgeTarget;
struct MediaAvStartupCoordinatorNodePreparation;

class MediaAvStartupCoordinatorNode final : public FFmpegNodeRuntime {
public:
    MediaAvStartupCoordinatorNode(
        MediaNodeId nodeId,
        MediaAvStartupCoordinatorNodePreparation preparation);
    ~MediaAvStartupCoordinatorNode() override;
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    enum class PendingInput {
        Audio,
        Video,
        Clock
    };

    ::media::Result<bool> fillPendingMedia(MediaGraphExecutionContext& context,
                                           const char* portName,
                                           std::deque<MediaBufferRef>& pending);
    ::media::Result<bool> fillSnapshotBarrierMedia(MediaGraphExecutionContext& context,
                                                   const char* portName,
                                                   std::deque<MediaBufferRef>& pending,
                                                   std::size_t& remaining);
    ::media::Result<bool> fillPendingClock(MediaGraphExecutionContext& context);
    ::media::Result<bool> fillTerminalBarrierClock(
        MediaGraphExecutionContext& context);
    ::media::Status activateClockBarrier(MediaGraphExecutionContext& context);
    ::media::Status sealClockBarrier(MediaGraphExecutionContext& context);
    ::media::Status activateTerminalBarrier(MediaGraphExecutionContext& context);
    ::media::Result<std::optional<PendingInput>> selectPending() const;
    ::media::Result<MediaNodeProcessResult> processOne(
        MediaGraphExecutionContext& context,
        PendingInput input);
    ::media::Result<MediaNodeProcessResult> processClock();
    ::media::Result<MediaNodeProcessResult> processControl(
        MediaGraphExecutionContext& context,
        PendingInput input,
        const MediaControlBuffer& control);
    ::media::Result<std::optional<MediaBufferRef>> prepareOutput(
        MediaGraphExecutionContext& context,
        const MediaAvStartupDecision& decision,
        const MediaAvStartupEnvelopeBuffer& envelope);
    void erasePurged(const std::vector<MediaAvStartupUnitId>& purged) noexcept;
    void clearTransientState() noexcept;

    std::unique_ptr<MediaAvStartupCoordinator> m_coordinator;
    std::shared_ptr<MediaAvStartupGenerationState> m_generationState;
    int m_outputAudioSampleRate = 0;
    std::deque<MediaBufferRef> m_pendingVideo;
    std::deque<MediaBufferRef> m_pendingAudio;
    MediaBufferRef m_pendingClock;
    bool m_clockBarrierActive = false;
    bool m_clockBarrierSnapshotSealed = false;
    std::size_t m_videoClockBarrierRemaining = 0;
    std::size_t m_audioClockBarrierRemaining = 0;
    bool m_terminalBarrierActive = false;
    std::size_t m_videoTerminalBarrierRemaining = 0;
    std::size_t m_audioTerminalBarrierRemaining = 0;
    std::size_t m_clockTerminalBarrierRemaining = 0;
    std::optional<MediaRunningTime> m_lastVideoObservedAt;
    std::optional<MediaRunningTime> m_lastAudioObservedAt;
    std::optional<MediaRunningTime> m_lastClock;
    bool m_terminalControlCommitted = false;
    bool m_keyTraceEmitted = false;
    std::optional<std::uint64_t> m_lastReleasedGeneration;
    std::optional<::media::ErrorInfo> m_deferredTerminalError;
};

} // namespace media::ffmpeg::graph
