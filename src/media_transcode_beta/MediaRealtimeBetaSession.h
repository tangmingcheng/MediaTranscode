#pragma once

#include "application/realtime/MediaRealtimeVideoRunController.h"
#include "media_transcode/Result.h"
#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"
#include "media_transcode_beta/MediaRealtimeBetaTemporaryDescription.h"
#include "media_transcode_beta/realtime.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace media::beta {

class MediaRealtimeBetaSession final {
public:
    explicit MediaRealtimeBetaSession(
        MediaRealtimeBetaOwnedConfig config) noexcept;
    ~MediaRealtimeBetaSession() noexcept;

    MediaRealtimeBetaSession(const MediaRealtimeBetaSession&) = delete;
    MediaRealtimeBetaSession& operator=(const MediaRealtimeBetaSession&) = delete;

    ::media::Status start();
    void requestStop() noexcept;
    mt_beta_realtime_snapshot snapshot() const noexcept;
    bool isCurrentThreadEventThread() const noexcept;

private:
    struct SessionFailure final {
        ::media::ErrorInfo error;
        mt_beta_failure_stage stage;
        mt_beta_completion_reason completionReason;
    };

    void eventThreadMain() noexcept;
    void runOnEventThread();
    void handlePrepared(
        const ffmpeg::graph::MediaRealtimeVideoPreparedReport& report);
    void handleProgress(
        const ffmpeg::graph::MediaGraphRuntimeReport& report);
    ::media::Status publishOutputDescription(bool finalAttempt);
    void handleOutcome(
        const ffmpeg::graph::MediaRealtimeVideoRunOutcome& outcome);

    void recordFirstFailure(SessionFailure failure);
    void finishFailure();
    void finishSuccess(mt_beta_completion_reason completionReason);
    void transitionState(mt_beta_realtime_state state);
    void setCompletionReason(mt_beta_completion_reason completionReason);
    void invokeCallback(const mt_beta_realtime_event& event) noexcept;
    void emitState(mt_beta_realtime_state state) noexcept;
    void emitOutputReady() noexcept;
    void emitError(const SessionFailure& failure);
    void emitCompleted(mt_beta_completion_reason completionReason) noexcept;
    std::chrono::milliseconds runningTime() const noexcept;

    MediaRealtimeBetaOwnedConfig m_config;
    ffmpeg::graph::MediaRealtimeVideoRunControl m_runControl;
    std::atomic<bool> m_startInvoked{false};
    std::atomic<bool> m_stopRequested{false};
    std::jthread m_eventThread;

    mutable std::mutex m_eventThreadIdentityMutex;
    std::thread::id m_eventThreadId;
    mutable std::mutex m_snapshotMutex;
    mt_beta_realtime_snapshot m_snapshot;

    mt_beta_realtime_event_callback m_callback = nullptr;
    void* m_callbackUserData = nullptr;
    std::optional<MediaRealtimeBetaTemporaryDescription> m_description;
    std::string m_outputDescriptionPath;
    std::string m_outputDescription;
    std::string m_eventDetail;
    std::optional<SessionFailure> m_firstFailure;
    std::chrono::steady_clock::time_point m_startedAt;
    bool m_outputReady = false;
    bool m_runningStateEmitted = false;
    bool m_terminalStateEmitted = false;
};

} // namespace media::beta
