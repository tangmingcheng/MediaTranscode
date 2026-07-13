#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"

#include <memory>
#include <deque>
#include <optional>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaAvStartupEnvelopeBuffer;
class MediaControlBuffer;

class MediaAvStartupCoordinatorNode final : public FFmpegNodeRuntime {
public:
    explicit MediaAvStartupCoordinatorNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
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

    ::media::Status configure(MediaGraphExecutionContext& context);
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
        const MediaAvStartupDecision& decision,
        const MediaAvStartupEnvelopeBuffer& envelope);
    void erasePurged(const std::vector<MediaAvStartupUnitId>& purged) noexcept;
    void resetState() noexcept;

    std::unique_ptr<MediaAvStartupCoordinator> m_coordinator;
    std::unordered_map<MediaAvStartupUnitId, MediaBufferRef,
                       MediaAvStartupUnitIdHash> m_payloads;
    std::deque<MediaBufferRef> m_pendingVideo;
    std::deque<MediaBufferRef> m_pendingAudio;
    MediaBufferRef m_pendingClock;
    bool m_clockBarrierActive = false;
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
    std::optional<::media::ErrorInfo> m_deferredTerminalError;
};

} // namespace media::ffmpeg::graph
