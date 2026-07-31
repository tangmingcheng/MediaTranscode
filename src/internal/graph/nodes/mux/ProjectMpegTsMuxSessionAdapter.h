#pragma once

#include "internal/graph/nodes/mux/MediaMuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpContinuityState.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <memory>
#include <optional>
#include <atomic>

namespace media::ffmpeg::graph {

class MediaOutputByteSink;
class MediaAvGenerationPurgeTarget;
class MediaProtocolOutputGenerationState;
class MediaTsMuxSession;
class MediaUdpDatagramSenderPortFactory;
struct MediaProjectMpegTsRuntimeOutputPlan;

class ProjectMpegTsGenerationSessionState final
    : public MediaProtocolOutputGenerationSessionState {
public:
    ProjectMpegTsGenerationSessionState();
    ~ProjectMpegTsGenerationSessionState() override;

private:
    friend class ProjectMpegTsMuxSessionAdapter;
    void resetForGenerationPurge() noexcept override;

    enum class State : std::uint8_t {
        Acquiring,
        Active,
        Finished,
        Poisoned
    };
    State state = State::Acquiring;
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan;
    std::optional<MediaPlaybackEpoch> epoch;
    std::optional<MediaAvSyncGroupKey> group;
    std::optional<MediaAvSyncGroupKey> plannedGroup;
    std::unique_ptr<MediaTsMuxSession> session;
    std::shared_ptr<MediaMpegTsRtpContinuityState> rtpContinuity;
    std::optional<MediaRunningTime> nextTransportDeadline;
    std::optional<MediaRunningTime> latestAcceptedEmission;
    bool mediaTimelineStarted = false;
    std::atomic<std::uint64_t> generation{0};
};

struct ProjectMpegTsGenerationAuthority final {
    static ::media::Result<ProjectMpegTsGenerationAuthority> create(
        std::shared_ptr<MediaProtocolOutputGenerationState> generationState,
        std::shared_ptr<ProjectMpegTsGenerationSessionState>
            generationSession);

    const std::shared_ptr<MediaProtocolOutputGenerationState>&
    generationState() const noexcept;
    const std::shared_ptr<ProjectMpegTsGenerationSessionState>&
    generationSession() const noexcept;

private:
    ProjectMpegTsGenerationAuthority(
        std::shared_ptr<MediaProtocolOutputGenerationState> generationState,
        std::shared_ptr<ProjectMpegTsGenerationSessionState>
            generationSession);

    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::shared_ptr<ProjectMpegTsGenerationSessionState>
        m_generationSession;
};

class ProjectMpegTsMuxSessionAdapter final : public MediaMuxSession {
public:
    explicit ProjectMpegTsMuxSessionAdapter(
        ProjectMpegTsGenerationAuthority authority);
    ~ProjectMpegTsMuxSessionAdapter() override;

    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;

    ::media::Status bindResource(MediaGraphExecutionContext& context,
                                 const MediaBufferRef& buffer) override;
    ::media::Status bindStreamConfig(MediaGraphExecutionContext& context,
                                     const MediaBufferRef& buffer) override;
    ::media::Status write(MediaGraphExecutionContext& context,
                          const MediaBufferRef& buffer) override;
    ::media::Result<MediaMuxSessionPollResult> poll(
        MediaGraphExecutionContext& context) override;
    bool bindingsReady() const noexcept override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status finish(MediaGraphExecutionContext& context) override;
    void abort() noexcept override;

private:
    using State = ProjectMpegTsGenerationSessionState::State;
    ::media::Status bindRuntimePlan(MediaGraphExecutionContext& context,
                                    const MediaBufferRef& buffer);
    ::media::Status bindSink(const MediaBufferRef& buffer);
    ::media::Status tryActivate(MediaGraphExecutionContext& context);
    ::media::Status validateAccessUnitLocked(
        const MediaBufferRef& buffer) const;
    ::media::Status fail(::media::ErrorInfo error);
    ::media::Status terminalStatus() const;
    void closeOwnedResources() noexcept;

    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::shared_ptr<ProjectMpegTsGenerationSessionState> m_generationSession;
    std::optional<MediaAvSyncGroupKey>& m_plannedGroup;
    State& m_state;
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>&
        m_outputPlan;
    std::optional<MediaPlaybackEpoch>& m_epoch;
    std::optional<MediaAvSyncGroupKey>& m_group;
    std::unique_ptr<MediaTsMuxSession>& m_session;
    std::shared_ptr<MediaMpegTsRtpContinuityState>& m_rtpContinuity;
    std::optional<MediaRunningTime>& m_nextTransportDeadline;
    std::optional<MediaRunningTime>& m_latestAcceptedEmission;
    bool& m_mediaTimelineStarted;
    std::atomic<std::uint64_t>& m_generation;
    std::unique_ptr<MediaOutputByteSink> m_sink;
    MediaBufferRef m_videoConfig;
    MediaBufferRef m_audioConfig;
    std::optional<::media::ErrorInfo> m_failure;
    bool m_resourcesClosed = false;
};

} // namespace media::ffmpeg::graph
