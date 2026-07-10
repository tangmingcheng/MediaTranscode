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

::media::Status AvPacketStartBarrierNode::onProcess(MediaGraphExecutionContext& context)
{
    auto configured = configure(context);
    if (!configured) {
        return configured;
    }

    if (auto status = processCodec(context, "video_codec", "video_codec"); !status) return status;
    if (auto status = processCodec(context, "audio_codec", "audio_codec"); !status) return status;
    if (auto status = processPacket(context, "video_packet", "video_packet", m_expectVideo, m_videoReady, m_pendingVideo); !status) return status;
    if (auto status = processPacket(context, "audio_packet", "audio_packet", m_expectAudio, m_audioReady, m_pendingAudio); !status) return status;
    return releaseIfReady(context);
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

::media::Status AvPacketStartBarrierNode::processCodec(MediaGraphExecutionContext& context,
                                                       const char* inputPort,
                                                       const char* outputPort)
{
    auto input = tryPopInputOptional(context, inputPort);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }
    return emitOutput(context, outputPort, *input.value());
}

::media::Status AvPacketStartBarrierNode::processPacket(MediaGraphExecutionContext& context,
                                                        const char* inputPort,
                                                        const char* outputPort,
                                                        bool expected,
                                                        bool& ready,
                                                        MediaBufferRef& pending)
{
    if (!expected) {
        return ::media::Status::success();
    }

    auto input = tryPopInputOptional(context, inputPort);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = *input.value();
    if (m_open) {
        return emitOutput(context, outputPort, buffer);
    }
    if (m_requireVideoKeyFrame && buffer->streamKind() == MediaStreamKind::Video && !buffer->isKeyFrame()) {
        return ::media::Status::success();
    }

    pending = buffer;
    ready = true;
    return ::media::Status::success();
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
    m_pendingVideo.reset();
    m_pendingAudio.reset();
}

} // namespace media::ffmpeg::graph
