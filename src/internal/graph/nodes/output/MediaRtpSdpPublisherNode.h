#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"
#include "internal/graph/runtime/filesystem/MediaAtomicUtf8FilePublisher.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include <memory>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaRtpSdpPublisherNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<std::unique_ptr<MediaRtpSdpPublisherNode>> create(
        MediaNodeId nodeId,
        MediaTranscodeStreamSet streamSet,
        std::string path,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
        std::unique_ptr<MediaAtomicFileReplacePort> replacePort);

    static MediaNodeKind staticKind() noexcept;
    static constexpr const char* generationPurgeIdentity() noexcept { return "rtp_sdp_generation_state"; }
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
    MediaRtpSdpPublisherNode(
        MediaNodeId nodeId,
        bool videoOnly,
        std::string path,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
        std::unique_ptr<MediaAtomicFileReplacePort> replacePort);

    ::media::Status validatePorts(MediaGraphExecutionContext& context) const;
    ::media::Result<bool> acquire(
        MediaGraphExecutionContext& context,
        const char* port,
        MediaScheduledStream expectedStream,
        MediaBufferRef& destination);
    ::media::Result<MediaNodeProcessResult> publish();
    ::media::Result<MediaNodeProcessResult> failTerminal(
        ::media::ErrorInfo error);
    void resetState() noexcept;
    ::media::Status applyGenerationPurge(MediaGraphExecutionContext& context, const MediaAvGenerationPurge& purge);
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_generationPurge;
    std::optional<MediaAvGenerationPurge> m_completedPurge;

    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_authority;
    bool m_videoOnly;
    std::string m_path;
    std::unique_ptr<MediaAtomicFileReplacePort> m_replacePort;
    MediaBufferRef m_video;
    MediaBufferRef m_audio;
    std::optional<MediaSdpSessionIdentity> m_publishedSession;
    std::optional<MediaRtpSdpMediaDescription> m_publishedVideo;
    std::optional<MediaRtpSdpMediaDescription> m_publishedAudio;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    bool m_published = false;
};

} // namespace media::ffmpeg::graph
