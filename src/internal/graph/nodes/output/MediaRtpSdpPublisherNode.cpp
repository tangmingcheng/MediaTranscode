#include "internal/graph/nodes/output/MediaRtpSdpPublisherNode.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"

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

bool sameCodec(const MediaSdpCodecDescription& left,
               const MediaSdpCodecDescription& right) noexcept
{
    if (left.index() != right.index()) return false;
    if (const auto* h264 =
            std::get_if<MediaH264SdpCodecDescription>(&left)) {
        const auto& other = std::get<MediaH264SdpCodecDescription>(right);
        return h264->profileLevelId() == other.profileLevelId() &&
            h264->spropParameterSets() == other.spropParameterSets() &&
            h264->packetizationMode() == other.packetizationMode();
    }
    if (const auto* hevc =
            std::get_if<MediaHevcSdpCodecDescription>(&left)) {
        const auto& other = std::get<MediaHevcSdpCodecDescription>(right);
        return hevc->spropVps() == other.spropVps() &&
            hevc->spropSps() == other.spropSps() &&
            hevc->spropPps() == other.spropPps();
    }
    const auto& aac = std::get<MediaAacLatmSdpCodecDescription>(left);
    const auto& other = std::get<MediaAacLatmSdpCodecDescription>(right);
    return aac.sampleRate() == other.sampleRate() &&
        aac.channels() == other.channels() &&
        aac.profileLevelId() == other.profileLevelId() &&
        aac.configurationPresent() == other.configurationPresent() &&
        aac.streamMuxConfigHex() == other.streamMuxConfigHex();
}

bool sameMedia(const MediaRtpSdpMediaDescription& left,
               const MediaRtpSdpMediaDescription& right) noexcept
{
    const auto& a = left.identity();
    const auto& b = right.identity();
    return a.kind() == b.kind() &&
        a.addressFamily() == b.addressFamily() &&
        a.remoteRtpNumericAddress() == b.remoteRtpNumericAddress() &&
        a.remoteRtcpNumericAddress() == b.remoteRtcpNumericAddress() &&
        a.remoteRtpPort() == b.remoteRtpPort() &&
        a.remoteRtcpPort() == b.remoteRtcpPort() &&
        a.payloadType() == b.payloadType() && a.ssrc() == b.ssrc() &&
        a.clockRate() == b.clockRate() && a.channels() == b.channels() &&
        sameCodec(left.codec(), right.codec());
}

} // namespace

MediaRtpSdpPublisherNode::MediaRtpSdpPublisherNode(
    MediaNodeId nodeId,
    bool videoOnly,
    std::string path,
    std::unique_ptr<MediaAtomicFileReplacePort> replacePort)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaRtpSdpPublisherNode"),
      m_videoOnly(videoOnly),
      m_path(std::move(path)),
      m_replacePort(std::move(replacePort))
{
}

::media::Result<std::unique_ptr<MediaRtpSdpPublisherNode>>
MediaRtpSdpPublisherNode::create(
    MediaNodeId nodeId,
    MediaTranscodeStreamSet streamSet,
    std::string path,
    std::unique_ptr<MediaAtomicFileReplacePort> replacePort)
{
    using Result = ::media::Result<std::unique_ptr<MediaRtpSdpPublisherNode>>;
    auto encodedStreamSet = MediaTranscodeStreamSetCodec::encode(streamSet);
    if (!nodeId.isValid() || path.empty() ||
        path.find('\0') != std::string::npos || !replacePort ||
        !encodedStreamSet) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP SDP publisher requires a stream set, path, and atomic port"));
    }
    bool videoOnly = false;
    switch (streamSet) {
    case MediaTranscodeStreamSet::AudioVideo:
        videoOnly = false;
        break;
    case MediaTranscodeStreamSet::VideoOnly:
        videoOnly = true;
        break;
    default:
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP SDP publisher rejects an unknown stream set"));
    }
    auto node = std::unique_ptr<MediaRtpSdpPublisherNode>(
        new (std::nothrow) MediaRtpSdpPublisherNode(
            nodeId, videoOnly, std::move(path), std::move(replacePort)));
    return node ? Result::success(std::move(node))
                : Result::failure(::media::ErrorInfo::allocationFailed(
                      "MediaRtpSdpPublisherNode"));
}

MediaNodeKind MediaRtpSdpPublisherNode::staticKind() noexcept
{
    return MediaNodeKind::RtpSdpPublisher;
}

::media::Status MediaRtpSdpPublisherNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const auto inputs = context.inputChannels(nodeId());
    const MediaChannel* video = context.findInputChannel(nodeId(), "video");
    const MediaChannel* audio = context.findInputChannel(nodeId(), "audio");
    if (inputs.size() != (m_videoOnly ? 1u : 2u) ||
        !context.outputChannels(nodeId()).empty() || !video ||
        video->binding().streamKind != MediaStreamKind::Metadata ||
        (m_videoOnly ? audio != nullptr
                   : !audio || audio->binding().streamKind !=
                                  MediaStreamKind::Metadata)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP SDP publisher ports conflict with its stream set"));
    }
    return ::media::Status::success();
}

::media::Status MediaRtpSdpPublisherNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Result<bool> MediaRtpSdpPublisherNode::acquire(
    MediaGraphExecutionContext& context,
    const char* port,
    MediaScheduledStream expectedStream,
    MediaBufferRef& destination)
{
    auto input = tryPopInputOptional(context, port);
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    if (destination) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP SDP publisher rejects duplicate descriptions"));
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
                      "RTP SDP publisher received abort")
                : ::media::ErrorInfo::invalidArgument(
                      "RTP SDP publisher requires a typed sender description"));
    }
    destination = std::move(*input.value());
    return ::media::Result<bool>::success(true);
}

::media::Result<MediaNodeProcessResult> MediaRtpSdpPublisherNode::publish()
{
    const auto* video = dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
        m_video.get());
    const auto* audio = dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
        m_audio.get());
    if (!video || (!m_videoOnly &&
                   (!audio || video->generation() != audio->generation() ||
                    !sameSession(video->session(), audio->session())))) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "RTP SDP descriptions conflict with their stream set/session"));
    }
    std::vector<MediaRtpSdpMediaDescription> media;
    media.reserve(m_videoOnly ? 1u : 2u);
    media.push_back(video->media());
    if (audio) media.push_back(audio->media());
    auto description = MediaRtpSdpDescription::create(
        video->session(), std::move(media));
    if (!description) return failTerminal(description.error());
    const bool unchanged = m_publishedSession && m_publishedVideo &&
        sameSession(*m_publishedSession, video->session()) &&
        sameMedia(*m_publishedVideo, video->media()) &&
        (m_videoOnly || (m_publishedAudio && audio &&
                       sameMedia(*m_publishedAudio, audio->media())));
    if (!unchanged) {
        auto serialized = MediaRtpSdpSerializer::serialize(description.value());
        if (!serialized) return failTerminal(serialized.error());
        MediaAtomicUtf8FilePublisher publisher(*m_replacePort);
        auto published = publisher.publish(m_path, serialized.value());
        if (!published) return failTerminal(published.error());
        m_publishedSession = video->session();
        m_publishedVideo = video->media();
        m_publishedAudio = audio ? std::optional(audio->media()) : std::nullopt;
    }
    m_published = true;
    m_video.reset();
    m_audio.reset();
    return processProgress();
}

::media::Result<MediaNodeProcessResult> MediaRtpSdpPublisherNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            *m_terminalFailure);
    }
    auto video = acquire(
        context, "video", MediaScheduledStream::Video, m_video);
    if (!video) return failTerminal(video.error());
    bool audioProgress = false;
    if (!m_videoOnly) {
        auto audio = acquire(
            context, "audio", MediaScheduledStream::Audio, m_audio);
        if (!audio) return failTerminal(audio.error());
        audioProgress = audio.value();
    }
    if (m_video &&
        (m_videoOnly || m_audio)) {
        return publish();
    }
    const MediaChannel* videoChannel =
        context.findInputChannel(nodeId(), "video");
    const MediaChannel* audioChannel =
        context.findInputChannel(nodeId(), "audio");
    const bool anyAborted = (videoChannel && videoChannel->aborted()) ||
        (!m_videoOnly &&
         audioChannel && audioChannel->aborted());
    if (anyAborted) {
        return failTerminal(::media::ErrorInfo::cancelled(
            "RTP SDP publisher input was aborted"));
    }
    const bool allClosed = videoChannel && videoChannel->closed() &&
        (m_videoOnly ||
         (audioChannel && audioChannel->closed()));
    if (allClosed) {
        if (m_video || m_audio || !m_published) {
            return failTerminal(::media::ErrorInfo::notInitialized(
                "RTP SDP publisher closed without a complete description"));
        }
        return processFinished();
    }
    return (video.value() || audioProgress) ? processProgress()
                                             : processWaiting();
}

::media::Result<MediaNodeProcessResult>
MediaRtpSdpPublisherNode::failTerminal(::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    return ::media::Result<MediaNodeProcessResult>::failure(*m_terminalFailure);
}

void MediaRtpSdpPublisherNode::resetState() noexcept
{
    m_video.reset();
    m_audio.reset();
    m_publishedSession.reset();
    m_publishedVideo.reset();
    m_publishedAudio.reset();
    m_terminalFailure.reset();
    m_published = false;
}

::media::Status MediaRtpSdpPublisherNode::flush(
    MediaGraphExecutionContext& context)
{
    cancelPendingOutputTransfer();
    resetState();
    return FFmpegNodeRuntime::flush(context);
}

::media::Status MediaRtpSdpPublisherNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaRtpSdpPublisherNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    m_terminalFailure = ::media::ErrorInfo::cancelled(
        "RTP SDP publisher was aborted");
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
