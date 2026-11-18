#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/filesystem/MediaAtomicUtf8FilePublisher.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaMpegTsRtpSdpPublisherNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<
        std::unique_ptr<MediaMpegTsRtpSdpPublisherNode>> create(
        MediaNodeId nodeId,
        MediaProtocolOutputSessionKey plannedSession,
        MediaTranscodeStreamSet streamSet,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
        std::unique_ptr<MediaAtomicFileReplacePort> replacePort);

    static MediaNodeKind staticKind() noexcept;
    static constexpr const char* generationPurgeIdentity() noexcept { return "mpegts_sdp_generation_state"; }
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept { return m_generationPurge; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    MediaMpegTsRtpSdpPublisherNode(
        MediaNodeId nodeId,
        MediaProtocolOutputSessionKey plannedSession,
        MediaTranscodeStreamSet streamSet,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
        std::unique_ptr<MediaAtomicFileReplacePort> replacePort);

    ::media::Status validatePorts(
        MediaGraphExecutionContext& context) const;
    ::media::Result<MediaNodeProcessResult> failTerminal(
        ::media::ErrorInfo error);
    void resetState() noexcept;
    ::media::Status applyGenerationPurge(MediaGraphExecutionContext& context, const MediaAvGenerationPurge& purge);
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_generationPurge;
    std::optional<MediaAvGenerationPurge> m_completedPurge;

    MediaBufferRef m_pendingPlan;
    MediaProtocolOutputSessionKey m_plannedSession;
    MediaTranscodeStreamSet m_streamSet;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_authority;
    std::unique_ptr<MediaAtomicFileReplacePort> m_replacePort;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    std::optional<std::uint64_t> m_lastPublishedGeneration;
};

} // namespace media::ffmpeg::graph
