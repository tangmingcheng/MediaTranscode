#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"
#include "internal/graph/runtime/filesystem/MediaAtomicUtf8FilePublisher.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

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
        std::unique_ptr<MediaAtomicFileReplacePort> replacePort);

    static MediaNodeKind staticKind() noexcept;
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
