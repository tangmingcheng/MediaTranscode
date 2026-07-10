#include "internal/graph/nodes/packet/AvPacketStartBarrierNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

namespace {

constexpr const char* ExpectVideoOption = "av_start_barrier.expect_video";
constexpr const char* ExpectAudioOption = "av_start_barrier.expect_audio";
constexpr const char* RequireVideoKeyFrameOption = "av_start_barrier.require_video_key_frame";

} // namespace

AvPacketStartBarrierNode::AvPacketStartBarrierNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AvPacketStartBarrierNode")
{
}

MediaNodeKind AvPacketStartBarrierNode::staticKind() noexcept
{
    return MediaNodeKind::AvPacketStartBarrier;
}

::media::Result<MediaNodeProcessResult> AvPacketStartBarrierNode::onProcess(MediaGraphExecutionContext& context)
{
    auto configured = configure(context);
    if (!configured) {
        return processProgress(configured);
    }

    bool progressed = false;
    auto videoCodec = processCodec(context, "video_codec", "video_codec");
    if (!videoCodec) return ::media::Result<MediaNodeProcessResult>::failure(videoCodec.error());
    progressed = progressed || videoCodec.value();
    auto audioCodec = processCodec(context, "audio_codec", "audio_codec");
    if (!audioCodec) return ::media::Result<MediaNodeProcessResult>::failure(audioCodec.error());
    progressed = progressed || audioCodec.value();
    auto videoPacket = processPacket(context, "video_packet", "video_packet", m_expectVideo, m_videoReady, m_videoEof, m_pendingVideo);
    if (!videoPacket) return ::media::Result<MediaNodeProcessResult>::failure(videoPacket.error());
    progressed = progressed || videoPacket.value();
    auto audioPacket = processPacket(context, "audio_packet", "audio_packet", m_expectAudio, m_audioReady, m_audioEof, m_pendingAudio);
    if (!audioPacket) return ::media::Result<MediaNodeProcessResult>::failure(audioPacket.error());
    progressed = progressed || audioPacket.value();
    auto released = releaseIfReady(context);
    if (!released) return ::media::Result<MediaNodeProcessResult>::failure(released.error());
    if (released && (!m_expectVideo || m_videoEof) && (!m_expectAudio || m_audioEof)) {
        return processFinished(released);
    }
    return progressed ? processProgress(released) : processWaiting();
}

::media::Status AvPacketStartBarrierNode::stop(MediaGraphExecutionContext& context)
{
    reset();
    return FFmpegNodeRuntime::stop(context);
}

void AvPacketStartBarrierNode::abort(MediaGraphExecutionContext& context) noexcept
{
    reset();
    FFmpegNodeRuntime::abort(context);
}

::media::Status AvPacketStartBarrierNode::configure(MediaGraphExecutionContext& context)
{
    if (m_configured) {
        return ::media::Status::success();
    }

    auto expectVideo = requiredBoolNodeOption(nodeOptions(context), "AvPacketStartBarrierNode", ExpectVideoOption);
    if (!expectVideo) {
        return ::media::Status::failure(expectVideo.error());
    }
    auto expectAudio = requiredBoolNodeOption(nodeOptions(context), "AvPacketStartBarrierNode", ExpectAudioOption);
    if (!expectAudio) {
        return ::media::Status::failure(expectAudio.error());
    }
    auto requireVideoKeyFrame = requiredBoolNodeOption(nodeOptions(context),
                                                      "AvPacketStartBarrierNode",
                                                      RequireVideoKeyFrameOption);
    if (!requireVideoKeyFrame) {
        return ::media::Status::failure(requireVideoKeyFrame.error());
    }
    if (!expectVideo.value() && !expectAudio.value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AvPacketStartBarrierNode requires at least one expected stream"));
    }
    if (requireVideoKeyFrame.value() && !expectVideo.value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AvPacketStartBarrierNode cannot require video key frame without video"));
    }

    m_expectVideo = expectVideo.value();
    m_expectAudio = expectAudio.value();
    m_requireVideoKeyFrame = requireVideoKeyFrame.value();
    m_videoReady = !m_expectVideo;
    m_audioReady = !m_expectAudio;
    m_configured = true;
    return ::media::Status::success();
}

::media::Result<bool> AvPacketStartBarrierNode::processCodec(MediaGraphExecutionContext& context,
                                                       const char* inputPort,
                                                       const char* outputPort)
{
    auto input = tryPopInputOptional(context, inputPort);
    if (!input) {
        return ::media::Result<bool>::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Result<bool>::success(false);
    }
    auto status = emitOutput(context, outputPort, *input.value());
    return status ? ::media::Result<bool>::success(true) : ::media::Result<bool>::failure(status.error());
}

::media::Result<bool> AvPacketStartBarrierNode::processPacket(MediaGraphExecutionContext& context,
                                                        const char* inputPort,
                                                        const char* outputPort,
                                                        bool expected,
                                                        bool& ready,
                                                        bool& eof,
                                                        MediaBufferRef& pending)
{
    if (!expected) {
        return ::media::Result<bool>::success(false);
    }

    auto input = tryPopInputOptional(context, inputPort);
    if (!input) {
        return ::media::Result<bool>::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Result<bool>::success(false);
    }

    MediaBufferRef buffer = *input.value();
    if (buffer->isEof()) {
        eof = true;
        ready = true;
        if (pending) {
            auto pendingStatus = emitOutput(context, outputPort, pending);
            if (!pendingStatus) return ::media::Result<bool>::failure(pendingStatus.error());
            pending.reset();
        }
        auto status = emitOutput(context, outputPort, buffer);
        return status ? ::media::Result<bool>::success(true) : ::media::Result<bool>::failure(status.error());
    }
    if (m_open) {
        auto status = emitOutput(context, outputPort, buffer);
        return status ? ::media::Result<bool>::success(true) : ::media::Result<bool>::failure(status.error());
    }
    if (m_requireVideoKeyFrame && buffer->streamKind() == MediaStreamKind::Video && !buffer->isKeyFrame()) {
        return ::media::Result<bool>::success(true);
    }

    pending = buffer;
    ready = true;
    return ::media::Result<bool>::success(true);
}

::media::Status AvPacketStartBarrierNode::releaseIfReady(MediaGraphExecutionContext& context)
{
    if (m_open || !m_videoReady || !m_audioReady) {
        return ::media::Status::success();
    }

    if (m_pendingVideo) {
        auto status = emitOutput(context, "video_packet", m_pendingVideo);
        if (!status) {
            return status;
        }
        m_pendingVideo.reset();
    }
    if (m_pendingAudio) {
        auto status = emitOutput(context, "audio_packet", m_pendingAudio);
        if (!status) {
            return status;
        }
        m_pendingAudio.reset();
    }

    m_open = true;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            "av_packet_start_barrier.open");
    return ::media::Status::success();
}

void AvPacketStartBarrierNode::reset() noexcept
{
    m_configured = false;
    m_expectVideo = false;
    m_expectAudio = false;
    m_requireVideoKeyFrame = false;
    m_open = false;
    m_videoReady = false;
    m_audioReady = false;
    m_videoEof = false;
    m_audioEof = false;
    m_pendingVideo.reset();
    m_pendingAudio.reset();
}

} // namespace media::ffmpeg::graph
