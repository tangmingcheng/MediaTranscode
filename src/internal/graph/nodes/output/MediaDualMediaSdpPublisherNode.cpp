#include "internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h"

#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpSerializer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

#include <new>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

bool sameSession(const MediaSdpSessionIdentity& left,
                 const MediaSdpSessionIdentity& right) noexcept
{
    return left.originUsername() == right.originUsername() &&
        left.sessionId() == right.sessionId() &&
        left.sessionVersion() == right.sessionVersion() &&
        left.sessionName() == right.sessionName() &&
        left.addressFamily() == right.addressFamily() &&
        left.numericAddress() == right.numericAddress() &&
        left.cname() == right.cname();
}

bool samePublishedSession(const MediaSdpSessionIdentity& left,
                          const MediaSdpSessionIdentity& right) noexcept
{
    return left.originUsername() == right.originUsername() &&
        left.sessionId() == right.sessionId() &&
        left.sessionName() == right.sessionName() &&
        left.addressFamily() == right.addressFamily() &&
        left.numericAddress() == right.numericAddress() &&
        left.cname() == right.cname();
}

bool sameCodec(const MediaSdpCodecDescription& left,
               const MediaSdpCodecDescription& right) noexcept
{
    if (left.index() != right.index()) return false;
    if (const auto* leftH264 =
            std::get_if<MediaH264SdpCodecDescription>(&left)) {
        const auto& rightH264 =
            std::get<MediaH264SdpCodecDescription>(right);
        return leftH264->profileLevelId() == rightH264.profileLevelId() &&
            leftH264->spropParameterSets() ==
                rightH264.spropParameterSets() &&
            leftH264->packetizationMode() ==
                rightH264.packetizationMode();
    }
    const auto& leftAac =
        std::get<MediaAacLatmSdpCodecDescription>(left);
    const auto& rightAac =
        std::get<MediaAacLatmSdpCodecDescription>(right);
    return leftAac.sampleRate() == rightAac.sampleRate() &&
        leftAac.channels() == rightAac.channels() &&
        leftAac.profileLevelId() == rightAac.profileLevelId() &&
        leftAac.configurationPresent() ==
            rightAac.configurationPresent() &&
        leftAac.streamMuxConfigHex() == rightAac.streamMuxConfigHex();
}

bool sameMedia(const MediaRtpSdpMediaDescription& left,
               const MediaRtpSdpMediaDescription& right) noexcept
{
    const auto& leftIdentity = left.identity();
    const auto& rightIdentity = right.identity();
    return leftIdentity.kind() == rightIdentity.kind() &&
        leftIdentity.addressFamily() == rightIdentity.addressFamily() &&
        leftIdentity.remoteRtpNumericAddress() ==
            rightIdentity.remoteRtpNumericAddress() &&
        leftIdentity.remoteRtcpNumericAddress() ==
            rightIdentity.remoteRtcpNumericAddress() &&
        leftIdentity.remoteRtpPort() == rightIdentity.remoteRtpPort() &&
        leftIdentity.remoteRtcpPort() == rightIdentity.remoteRtcpPort() &&
        leftIdentity.payloadType() == rightIdentity.payloadType() &&
        leftIdentity.ssrc() == rightIdentity.ssrc() &&
        leftIdentity.clockRate() == rightIdentity.clockRate() &&
        leftIdentity.channels() == rightIdentity.channels() &&
        sameCodec(left.codec(), right.codec());
}

} // namespace

MediaDualMediaSdpPublisherNode::MediaDualMediaSdpPublisherNode(
    MediaNodeId nodeId,
    std::string path,
    std::unique_ptr<MediaAtomicFileReplacePort> replacePort)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaDualMediaSdpPublisherNode"),
      m_path(std::move(path)),
      m_replacePort(std::move(replacePort))
{
}

::media::Result<std::unique_ptr<MediaDualMediaSdpPublisherNode>>
MediaDualMediaSdpPublisherNode::create(
    MediaNodeId nodeId,
    std::string path,
    std::unique_ptr<MediaAtomicFileReplacePort> replacePort)
{
    using NodeResult =
        ::media::Result<std::unique_ptr<MediaDualMediaSdpPublisherNode>>;
    if (!nodeId.isValid() || path.empty() || path.find('\0') != std::string::npos ||
        !replacePort) {
        return NodeResult::failure(::media::ErrorInfo::invalidArgument(
            "Dual-media SDP publisher requires a planned path and atomic replace port"));
    }
    try {
        return NodeResult::success(
            std::unique_ptr<MediaDualMediaSdpPublisherNode>(
                new MediaDualMediaSdpPublisherNode(
                    nodeId, std::move(path), std::move(replacePort))));
    } catch (const std::bad_alloc&) {
        return NodeResult::failure(::media::ErrorInfo::allocationFailed(
            "MediaDualMediaSdpPublisherNode"));
    }
}

MediaNodeKind MediaDualMediaSdpPublisherNode::staticKind() noexcept
{
    return MediaNodeKind::DualMediaSdpPublisher;
}

::media::Status MediaDualMediaSdpPublisherNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Status MediaDualMediaSdpPublisherNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const MediaChannel* video = context.findInputChannel(nodeId(), "video");
    const MediaChannel* audio = context.findInputChannel(nodeId(), "audio");
    if (context.inputChannels(nodeId()).size() != 2 ||
        !context.outputChannels(nodeId()).empty() || !video || !audio ||
        video->binding().streamKind != MediaStreamKind::Metadata ||
        audio->binding().streamKind != MediaStreamKind::Metadata) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Dual-media SDP publisher requires exactly two metadata inputs"));
    }
    return ::media::Status::success();
}

::media::Result<bool> MediaDualMediaSdpPublisherNode::acquire(
    MediaGraphExecutionContext& context,
    const char* port,
    MediaScheduledStream expectedStream,
    MediaBufferRef& destination)
{
    auto input = tryPopInputOptional(context, port);
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) {
        MediaChannel* channel = context.findInputChannel(nodeId(), port);
        if (channel && channel->aborted()) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::cancelled(
                    "Dual-media SDP publisher input was aborted"));
        }
        return ::media::Result<bool>::success(false);
    }
    if (destination) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Dual-media SDP publisher rejects duplicate descriptions"));
    }
    const auto* description =
        dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
            input.value()->get());
    if (!description || description->stream() != expectedStream) {
        const auto* control = dynamic_cast<const MediaControlBuffer*>(
            input.value()->get());
        return ::media::Result<bool>::failure(
            control && control->controlKind() == MediaControlBufferKind::Abort
                ? ::media::ErrorInfo::cancelled(
                      "Dual-media SDP publisher received abort")
                : ::media::ErrorInfo::invalidArgument(
                      "Dual-media SDP publisher requires typed opened descriptions"));
    }
    destination = std::move(*input.value());
    return ::media::Result<bool>::success(true);
}

::media::Result<MediaNodeProcessResult>
MediaDualMediaSdpPublisherNode::publish()
{
    const auto* video = dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
        m_video.get());
    const auto* audio = dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
        m_audio.get());
    if (!video || !audio || video->generation() != audio->generation() ||
        !sameSession(video->session(), audio->session())) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Dual-media SDP descriptions do not share one session identity"));
    }
    std::vector<MediaRtpSdpMediaDescription> media;
    media.reserve(2);
    media.push_back(video->media());
    media.push_back(audio->media());
    auto description = MediaRtpSdpDescription::create(
        video->session(), std::move(media));
    if (!description) return failTerminal(description.error());
    const bool unchanged =
        m_publishedSession && m_publishedVideo && m_publishedAudio &&
        samePublishedSession(*m_publishedSession, video->session()) &&
        sameMedia(*m_publishedVideo, video->media()) &&
        sameMedia(*m_publishedAudio, audio->media());
    if (!unchanged) {
        auto serialized =
            MediaRtpSdpSerializer::serialize(description.value());
        if (!serialized) return failTerminal(serialized.error());
        MediaAtomicUtf8FilePublisher publisher(*m_replacePort);
        auto published = publisher.publish(m_path, serialized.value());
        if (!published) return failTerminal(published.error());
        m_publishedSession = video->session();
        m_publishedVideo = video->media();
        m_publishedAudio = audio->media();
    }
    m_published = true;
    m_video.reset();
    m_audio.reset();
    return processProgress();
}

::media::Result<MediaNodeProcessResult>
MediaDualMediaSdpPublisherNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            *m_terminalFailure);
    }
    auto video = acquire(
        context, "video", MediaScheduledStream::Video, m_video);
    if (!video) return failTerminal(video.error());
    auto audio = acquire(
        context, "audio", MediaScheduledStream::Audio, m_audio);
    if (!audio) return failTerminal(audio.error());
    if (m_video && m_audio) {
        return publish();
    }
    const MediaChannel* videoChannel =
        context.findInputChannel(nodeId(), "video");
    const MediaChannel* audioChannel =
        context.findInputChannel(nodeId(), "audio");
    const bool videoClosed = videoChannel && videoChannel->closed();
    const bool audioClosed = audioChannel && audioChannel->closed();
    if (videoClosed && audioClosed) {
        if (m_video || m_audio) {
            return failTerminal(::media::ErrorInfo::notInitialized(
                "Dual-media SDP publisher lost one final description"));
        }
        return m_published
            ? processFinished()
            : failTerminal(::media::ErrorInfo::notInitialized(
                  "Dual-media SDP publisher received no descriptions"));
    }
    return (video.value() || audio.value()) ? processProgress()
                                            : processWaiting();
}

::media::Result<MediaNodeProcessResult>
MediaDualMediaSdpPublisherNode::failTerminal(::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    return ::media::Result<MediaNodeProcessResult>::failure(
        *m_terminalFailure);
}

void MediaDualMediaSdpPublisherNode::resetState() noexcept
{
    m_video.reset();
    m_audio.reset();
    m_publishedSession.reset();
    m_publishedVideo.reset();
    m_publishedAudio.reset();
    m_terminalFailure.reset();
    m_published = false;
}

::media::Status MediaDualMediaSdpPublisherNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

::media::Status MediaDualMediaSdpPublisherNode::flush(
    MediaGraphExecutionContext& context)
{
    cancelPendingOutputTransfer();
    resetState();
    return FFmpegNodeRuntime::flush(context);
}

void MediaDualMediaSdpPublisherNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_video.reset();
    m_audio.reset();
    m_published = false;
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Dual-media SDP publisher was aborted");
    }
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
