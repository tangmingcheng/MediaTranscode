#include "internal/graph/nodes/video/VideoDecodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"

extern "C" {
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

VideoDecodeNode::VideoDecodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoDecodeNode")
    , m_codecApi(makeMediaVideoDecoderCodecApi())
{
}

VideoDecodeNode::VideoDecodeNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoDecodeNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_codecApi(makeMediaVideoDecoderCodecApi())
{
}

VideoDecodeNode::VideoDecodeNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry,
    std::shared_ptr<MediaVideoDecoderCodecApi> codecApi)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoDecodeNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_codecApi(std::move(codecApi))
{
}

MediaNodeKind VideoDecodeNode::staticKind() noexcept
{
    return MediaNodeKind::VideoDecode;
}

std::string_view VideoDecodeNode::generationPurgeIdentity() noexcept
{
    return "video_decode";
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
VideoDecodeNode::generationPurgeTarget() const noexcept
{
    return m_lineageRegistry;
}

bool VideoDecodeNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    if (!m_lineageRegistry || !buffer || buffer->streamKind() != MediaStreamKind::Video ||
        buffer->isEof() || buffer->isFlush()) return true;
    const auto lineage = FFmpegFrameView::canonicalLineage(buffer);
    return lineage && m_lineageRegistry->retainedOutputIsCurrent(*lineage);
}

::media::Status VideoDecodeNode::start(MediaGraphExecutionContext& context) { resetRuntimeState(); return FFmpegCodecNodeRuntime::start(context); }
::media::Status VideoDecodeNode::stop(MediaGraphExecutionContext& context) { auto status = FFmpegCodecNodeRuntime::stop(context); resetRuntimeState(); return status; }
void VideoDecodeNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegCodecNodeRuntime::abort(context); resetRuntimeState(); }
void VideoDecodeNode::resetRuntimeState() noexcept
{
    m_terminals.reset(); m_eofEmitted = false; m_receivePending = false; m_flushPending = false;
    m_flushIsEof = false; m_flushSent = false; m_flushBuffer.reset(); m_pendingPacket.reset();
    m_pendingLineage.reset();
    m_lineageGenerations.clear();
}

::media::Status VideoDecodeNode::attachPendingLineage()
{
    if (!m_lineageRegistry) return ::media::Status::success();
    if (!m_pendingLineage || !m_pendingPacket || m_pendingPacket->opaque_ref)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("VideoDecodeNode requires one unowned canonical packet lineage"));
    auto token = m_lineageRegistry->submit(m_pendingLineage);
    if (!token) return ::media::Status::failure(token.error());
    auto opaque = makeMediaFfmpegLineageOpaque(std::move(token).value());
    if (!opaque) return ::media::Status::failure(opaque.error());
    m_pendingPacket->opaque_ref = opaque.value();
    m_lineageGenerations.insert(m_pendingLineage->generation);
    m_pendingLineage.reset();
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> VideoDecodeNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_flushPending) return continueFlush(context);
    if (m_receivePending) {
        auto receiveResult = receiveFrames(context);
        if (!receiveResult) return processProgress(::media::Status::failure(receiveResult.error()));
        m_receivePending = false;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_pendingPacket) return submitPendingPacket(context);
    if (m_terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    if (!hasCodecContext()) {
        auto codecInput = tryPopInputOptional(context, "codec");
        if (!codecInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(codecInput.error());
        }
        if (!codecInput.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        }
        if (!tryBindCodecContext(*codecInput.value())) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument("VideoDecodeNode expected codec context on codec input"));
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* packetInput = context.findInputChannel(nodeId(), "packet");
        if (packetInput && packetInput->closed()) {
            m_terminals.markClosed("packet");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const MediaBufferRef& buffer = *input.value();
    if (tryBindCodecContext(buffer)) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    if (buffer->isEof() || buffer->isFlush()) {
        const bool eof = buffer->isEof();
        if (eof && m_eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        m_flushPending = true;
        m_flushIsEof = eof;
        m_flushSent = false;
        m_flushBuffer = buffer;
        return continueFlush(context);
    }

    const AVPacket* packet = FFmpegPacketView::packet(buffer);
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("VideoDecodeNode expected packet buffer"));
    }
    m_pendingPacket.reset(av_packet_clone(packet));
    if (!m_pendingPacket) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed("VideoDecodeNode failed to clone input packet"));
    }
    if (m_lineageRegistry) {
        m_pendingLineage = FFmpegPacketView::canonicalLineage(buffer);
        if (!m_pendingLineage) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoDecodeNode requires canonical packet lineage"));
        }
    }

    return submitPendingPacket(context);
}

::media::Result<MediaNodeProcessResult> VideoDecodeNode::submitPendingPacket(
    MediaGraphExecutionContext& context)
{
    if (m_lineageRegistry && !m_pendingPacket->opaque_ref) {
        auto attached = attachPendingLineage();
        if (!attached) return processProgress(std::move(attached));
    }
    if (!m_codecApi) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("VideoDecodeNode codec API is missing"));
    }
    const int sendRet = m_codecApi->sendPacket(codecContext(), m_pendingPacket.get());
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(video)"));
    }
    if (sendRet == 0) m_pendingPacket.reset();

    auto receiveStatus = receiveFrames(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<bool> VideoDecodeNode::receiveFrames(MediaGraphExecutionContext& context)
{
    while (true) {
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::allocationFailed("VideoDecodeNode failed: av_frame_alloc returned null"));
        }

        const int ret = m_codecApi->receiveFrame(codecContext(), frame.get());
        if (ret == AVERROR(EAGAIN)) return ::media::Result<bool>::success(false);
        if (ret == AVERROR_EOF) return ::media::Result<bool>::success(true);

        if (ret < 0) {
            return ::media::Result<bool>::failure(FFmpegGraphError::fromCode(ret, "avcodec_receive_frame(video)"));
        }

        std::shared_ptr<const MediaCanonicalLineage> lineage;
        if (m_lineageRegistry) {
            auto resolved = m_lineageRegistry->resolveOutput(frame->opaque_ref);
            if (!resolved) return ::media::Result<bool>::failure(resolved.error());
            if (resolved.value()) lineage = std::move(*resolved.value());
            av_buffer_unref(&frame->opaque_ref);
            if (!lineage) continue;
        }
        auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video);
        if (!buffer) {
            return ::media::Result<bool>::failure(buffer.error());
        }

        MediaBufferRef output = buffer.value();
        if (lineage) {
            auto canonical = MediaCanonicalVideoFrameBuffer::create(output, std::move(lineage));
            if (!canonical) return ::media::Result<bool>::failure(canonical.error());
            output = std::move(canonical).value();
        }
        auto pushStatus = pushToMatchingOutputs(context, output, MediaStreamKind::Video);
        if (!pushStatus) {
            if (pushStatus.error().code == ::media::ErrorCode::WouldBlock && !m_flushPending) m_receivePending = true;
            return ::media::Result<bool>::failure(pushStatus.error());
        }
    }
}

::media::Result<MediaNodeProcessResult> VideoDecodeNode::continueFlush(MediaGraphExecutionContext& context)
{
    if (!m_flushSent) {
        const int sendRet = m_codecApi->sendPacket(codecContext(), nullptr);
        if (sendRet == 0 || sendRet == AVERROR_EOF) m_flushSent = true;
        else if (sendRet != AVERROR(EAGAIN))
            return ::media::Result<MediaNodeProcessResult>::failure(
                FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(video flush)"));
    }
    auto drain = receiveFrames(context);
    if (!drain) return processProgress(::media::Status::failure(drain.error()));
    if (!drain.value()) return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    if (!m_flushIsEof) m_codecApi->flushBuffers(codecContext());
    if (m_lineageRegistry) {
        for (const auto generation : m_lineageGenerations) {
            auto finished = m_lineageRegistry->finishGeneration(generation);
            if (!finished) return processProgress(std::move(finished));
        }
        m_lineageGenerations.clear();
    }
    const bool eof = m_flushIsEof;
    MediaBufferRef terminal = std::move(m_flushBuffer);
    m_flushPending = false; m_flushIsEof = false; m_flushSent = false;
    if (eof) { m_terminals.markEof("packet"); m_eofEmitted = true; }
    auto status = broadcastControlToAllOutputs(context, terminal);
    return eof ? processFinished(std::move(status)) : processProgress(std::move(status));
}

} // namespace media::ffmpeg::graph
