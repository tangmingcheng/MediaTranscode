#pragma once

#include "internal/graph/nodes/mux/MediaMuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaOutputByteSink;
class MediaAvGenerationPurgeTarget;
class MediaProtocolOutputGenerationState;
class MediaTsMuxSession;

class ProjectMpegTsMuxSessionAdapter final : public MediaMuxSession {
public:
    explicit ProjectMpegTsMuxSessionAdapter(
        std::shared_ptr<MediaProtocolOutputGenerationState> generationState);
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
    enum class State : std::uint8_t { Acquiring, Active, Finished, Poisoned };

    ::media::Status bindRuntimePlan(MediaGraphExecutionContext& context,
                                    const MediaBufferRef& buffer);
    ::media::Status bindSink(const MediaBufferRef& buffer);
    ::media::Status tryActivate(MediaGraphExecutionContext& context);
    ::media::Status validateExecutionBinding(
        MediaGraphExecutionContext& context) const;
    ::media::Status validateAccessUnit(const MediaBufferRef& buffer) const;
    ::media::Status permitRuntimePlanGeneration(
        std::uint64_t generation);
    bool outputPermitted(MediaGraphExecutionContext& context) const noexcept;
    void discardGenerationSession() noexcept;
    ::media::Status fail(::media::ErrorInfo error);
    ::media::Status terminalStatus() const;
    void closeOwnedResources() noexcept;

    State m_state = State::Acquiring;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::optional<MediaTsMuxPlan> m_plan;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaAvSyncGroupKey> m_group;
    std::unique_ptr<MediaOutputByteSink> m_sink;
    MediaBufferRef m_videoConfig;
    MediaBufferRef m_audioConfig;
    std::unique_ptr<MediaTsMuxSession> m_session;
    std::optional<::media::ErrorInfo> m_failure;
    std::optional<MediaRunningTime> m_nextTransportDeadline;
    bool m_mediaTimelineStarted = false;
    bool m_resourcesClosed = false;
};

} // namespace media::ffmpeg::graph
