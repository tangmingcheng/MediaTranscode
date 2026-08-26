#include "internal/graph/nodes/video/VideoDecodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

extern "C" {
#include <libavutil/error.h>
}

#include <utility>
#include <sstream>

namespace media::ffmpeg::graph {

VideoDecodeLineageState::VideoDecodeLineageState(
    std::shared_ptr<MediaCodecLineageRegistry> registry,
    std::shared_ptr<MediaVideoDecoderCodecApi> codecApi) noexcept
    : MediaVideoLineageState(std::move(registry))
    , m_codecApi(std::move(codecApi))
{
}

void VideoDecodeLineageState::bindCodec(
    MediaBufferRef owner, AVCodecContext* context) noexcept
{
    m_codecOwner = std::move(owner);
    m_codecContext = context;
}

void VideoDecodeLineageState::resetCodecBinding() noexcept
{
    m_codecContext = nullptr;
    m_codecOwner.reset();
}

void VideoDecodeLineageState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    if (m_codecApi && m_codecContext) {
        m_codecApi->flushBuffers(m_codecContext);
    }
    clearLineageStorage();
}

void VideoDecodeLineageState::clearLineageStorage() noexcept
{
    av_buffer_unref(&pendingSubmissionLineage);
    for (AVBufferRef* opaque : submissionOrderLineage) {
        av_buffer_unref(&opaque);
    }
    submissionOrderLineage.clear();
    terminals.reset();
    eofEmitted = false;
    receivePending = false;
    flushPending = false;
    flushIsEof = false;
    flushSent = false;
    flushBuffer.reset();
    pendingPacket.reset();
    pendingPayloadCredit.reset();
    pendingLineage.reset();
    lineageGenerations.clear();
}

void VideoDecodeLineageState::resetForLifecycle() noexcept
{
    auto lineageLock = lock();
    clearLineageStorage();
    resetGenerationLifecycle();
    resetCodecBinding();
}

VideoDecodeNode::VideoDecodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoDecodeNode")
    , m_codecApi(makeMediaVideoDecoderCodecApi())
    , m_lineageState(std::make_shared<VideoDecodeLineageState>(
          nullptr, m_codecApi))
{
}

VideoDecodeNode::VideoDecodeNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoDecodeNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_codecApi(makeMediaVideoDecoderCodecApi())
    , m_lineageState(std::make_shared<VideoDecodeLineageState>(
          m_lineageRegistry, m_codecApi))
{
}

VideoDecodeNode::VideoDecodeNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry,
    std::shared_ptr<MediaVideoDecoderCodecApi> codecApi)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoDecodeNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_codecApi(std::move(codecApi))
    , m_lineageState(std::make_shared<VideoDecodeLineageState>(
          m_lineageRegistry, m_codecApi))
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
    return m_lineageState->synchronized() ? m_lineageState : nullptr;
}

bool VideoDecodeNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto lineage = FFmpegFrameView::canonicalLineage(buffer);
    return m_lineageState->pendingOutputIsCurrent(
        buffer, lineage ? std::optional<std::uint64_t>(lineage->generation)
                        : std::nullopt);
}

::media::Status VideoDecodeNode::start(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    auto contract = MediaVideoFrameContractValidator::contractFromOptions(
        nodeOptions(context), "decoder.pipeline.output", "VideoDecodeNode");
    if (!contract) return ::media::Status::failure(contract.error());
    m_outputContract = std::move(contract).value();
    if (m_lineageRegistry) {
        auto copyOpaque = parseMediaVideoLineageCopyOpaqueOption(
            nodeOptions(context), "video.lineage.decoder_copy_opaque");
        if (!copyOpaque) return ::media::Status::failure(copyOpaque.error());
        m_copyOpaqueLineage = copyOpaque.value();
    }
    return FFmpegCodecNodeRuntime::start(context);
}
::media::Status VideoDecodeNode::stop(MediaGraphExecutionContext& context)
{
    std::ostringstream summary;
    summary << "video_decode.zero_copy_summary drm_prime=" << m_drmPrimeFrames
            << " software_frame=" << m_softwareFrames;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            summary.str());
    auto status = FFmpegCodecNodeRuntime::stop(context);
    resetRuntimeState();
    return status;
}
void VideoDecodeNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegCodecNodeRuntime::abort(context); resetRuntimeState(); }
void VideoDecodeNode::resetRuntimeState() noexcept
{
    m_firstPacketDiagnosticEmitted = false;
    m_firstSubmitDiagnosticEmitted = false;
    m_firstFrameDiagnosticEmitted = false;
    m_outputContract.reset();
    m_copyOpaqueLineage.reset();
    m_drmPrimeFrames = 0;
    m_softwareFrames = 0;
    m_lineageState->resetForLifecycle();
}

::media::Status VideoDecodeNode::attachPendingLineage()
{
    if (!m_lineageState->pendingPacket ||
        m_lineageState->pendingPacket->opaque_ref ||
        (!m_lineageState->pendingPayloadCredit && !m_lineageRegistry)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoDecodeNode requires one unowned packet payload credit"));
    }
    ::media::Result<AVBufferRef*> opaque = m_lineageRegistry
        ? [&]() -> ::media::Result<AVBufferRef*> {
              if (!m_lineageState->pendingLineage) {
                  return ::media::Result<AVBufferRef*>::failure(
                      ::media::ErrorInfo::invalidArgument(
                          "VideoDecodeNode requires canonical packet lineage"));
              }
              auto token = m_lineageRegistry->submit(
                  m_lineageState->pendingLineage);
              return token
                  ? makeMediaFfmpegCodecOpaque(
                        std::move(token).value(),
                        m_lineageState->pendingPayloadCredit)
                  : ::media::Result<AVBufferRef*>::failure(token.error());
          }()
        : makeMediaFfmpegCodecOpaque(
              m_lineageState->pendingPayloadCredit);
    if (!opaque) return ::media::Status::failure(opaque.error());
    if (!m_copyOpaqueLineage || *m_copyOpaqueLineage) {
        m_lineageState->pendingPacket->opaque_ref = opaque.value();
    } else {
        if (m_lineageState->pendingSubmissionLineage) {
            av_buffer_unref(&opaque.value());
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoDecodeNode already owns pending submission lineage"));
        }
        m_lineageState->pendingSubmissionLineage = opaque.value();
    }
    if (m_lineageState->pendingLineage) {
        m_lineageState->lineageGenerations.insert(
            m_lineageState->pendingLineage->generation);
    }
    m_lineageState->pendingLineage.reset();
    m_lineageState->pendingPayloadCredit.reset();
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> VideoDecodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    if (m_lineageState->flushPending) return continueFlush(context);
    if (m_lineageState->receivePending) {
        auto receiveResult = receiveFrames(context);
        if (!receiveResult) return processProgress(::media::Status::failure(receiveResult.error()));
        m_lineageState->receivePending = false;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_lineageState->pendingPacket) return submitPendingPacket(context);
    if (m_lineageState->terminals.finished()) {
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
        m_lineageState->bindCodec(*codecInput.value(), codecContext());
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* packetInput = context.findInputChannel(nodeId(), "packet");
        if (packetInput && packetInput->closed()) {
            m_lineageState->terminals.markClosed("packet");
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
        if (eof && m_lineageState->eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        m_lineageState->flushPending = true;
        m_lineageState->flushIsEof = eof;
        m_lineageState->flushSent = false;
        m_lineageState->flushBuffer = buffer;
        return continueFlush(context);
    }

    const AVPacket* packet = FFmpegPacketView::packet(buffer);
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("VideoDecodeNode expected packet buffer"));
    }
    const auto& inputPayloadCredit = FFmpegPacketView::payloadCredit(buffer);
    if (!inputPayloadCredit && context.payloadCreditsRequired()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoDecodeNode input packet lacks payload credit ownership"));
    }
    if (!m_firstPacketDiagnosticEmitted) {
        std::ostringstream out;
        out << "video_decode_trace stage=first_packet pts=" << packet->pts
            << " dts=" << packet->dts
            << " size=" << packet->size;
        if (const auto lineage = FFmpegPacketView::canonicalLineage(buffer)) {
            out << " generation=" << lineage->generation
                << " sequence=" << lineage->sourceSequence.value();
        }
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                out.str());
        m_firstPacketDiagnosticEmitted = true;
    }
    ::media::ffmpeg::PacketPtr pendingPacket(av_packet_clone(packet));
    if (!pendingPacket) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed("VideoDecodeNode failed to clone input packet"));
    }
    std::shared_ptr<const MediaCanonicalLineage> pendingLineage;
    if (m_lineageRegistry) {
        pendingLineage = FFmpegPacketView::canonicalLineage(buffer);
        if (!pendingLineage) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoDecodeNode requires canonical packet lineage"));
        }
        auto disposition = m_lineageState->classifyObservation(
            pendingLineage->generation);
        if (!disposition) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                disposition.error());
        }
        if (disposition.value() ==
            MediaVideoLineageGenerationDisposition::DropStale) {
            return processProgress();
        }
        if (auto status = m_lineageState->observe(pendingLineage->generation);
            !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }

    m_lineageState->pendingPacket = std::move(pendingPacket);
    m_lineageState->pendingPayloadCredit = inputPayloadCredit;
    m_lineageState->pendingLineage = std::move(pendingLineage);

    return submitPendingPacket(context);
}

::media::Result<MediaNodeProcessResult> VideoDecodeNode::submitPendingPacket(
    MediaGraphExecutionContext& context)
{
    if ((m_lineageRegistry || m_lineageState->pendingPayloadCredit) &&
        !m_lineageState->pendingPacket->opaque_ref &&
        !m_lineageState->pendingSubmissionLineage) {
        auto attached = attachPendingLineage();
        if (!attached) return processProgress(std::move(attached));
    }
    if (!m_codecApi) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("VideoDecodeNode codec API is missing"));
    }
    const int sendRet = m_codecApi->sendPacket(
        codecContext(), m_lineageState->pendingPacket.get());
    if (!m_firstSubmitDiagnosticEmitted) {
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            std::string("video_decode_trace stage=first_submit result=") +
                (sendRet == 0 ? "accepted" :
                 sendRet == AVERROR(EAGAIN) ? "would_block" : "failed"));
        m_firstSubmitDiagnosticEmitted = true;
    }
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(video)"));
    }
    if (sendRet == 0) {
        if (m_lineageState->pendingSubmissionLineage) {
            m_lineageState->submissionOrderLineage.push_back(
                m_lineageState->pendingSubmissionLineage);
            m_lineageState->pendingSubmissionLineage = nullptr;
        }
        m_lineageState->pendingPacket.reset();
    }

    auto receiveStatus = receiveFrames(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<bool> VideoDecodeNode::receiveFrames(MediaGraphExecutionContext& context)
{
    while (true) {
        auto reservation = context.reservePayload(
            nodeId(), MediaStreamKind::Video, MediaPayloadKind::Frame);
        if (!reservation) {
            return ::media::Result<bool>::failure(reservation.error());
        }
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
        if (!m_outputContract) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::notInitialized("VideoDecodeNode output frame contract is not bound"));
        }
        auto facts = MediaVideoFrameContractValidator::validate(
            *frame, *m_outputContract, "VideoDecodeNode decode output");
        if (!facts) return ::media::Result<bool>::failure(facts.error());
        m_drmPrimeFrames += facts.value().drmPrime ? 1U : 0U;
        m_softwareFrames += facts.value().software ? 1U : 0U;
        if (!m_firstFrameDiagnosticEmitted) {
            std::ostringstream out;
            out << "video_decode_trace stage=first_frame pts=" << frame->pts << ' '
                << MediaVideoFrameContractValidator::describe(*frame, facts.value());
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                    MediaGraphDiagnosticPhase::RuntimeNode,
                                    out.str());
            m_firstFrameDiagnosticEmitted = true;
        }

        std::shared_ptr<const MediaCanonicalLineage> lineage;
        if (m_lineageRegistry) {
            AVBufferRef* outputLineage = frame->opaque_ref;
            if (m_copyOpaqueLineage && !*m_copyOpaqueLineage) {
                if (m_lineageState->submissionOrderLineage.empty()) {
                    return ::media::Result<bool>::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "VideoDecodeNode received a frame without planned submission-order lineage"));
                }
                outputLineage = m_lineageState->submissionOrderLineage.front();
                m_lineageState->submissionOrderLineage.pop_front();
            }
            auto resolved = m_lineageRegistry->resolveOutput(outputLineage);
            if (!resolved) return ::media::Result<bool>::failure(resolved.error());
            if (resolved.value()) lineage = std::move(*resolved.value());
            if (outputLineage == frame->opaque_ref) {
                av_buffer_unref(&frame->opaque_ref);
            } else {
                av_buffer_unref(&outputLineage);
            }
            if (!lineage) continue;
        }
        auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video);
        if (!buffer) {
            return ::media::Result<bool>::failure(buffer.error());
        }
        const AVFrame* receivedFrame = FFmpegFrameView::frame(buffer.value());
        auto footprint = receivedFrame
            ? MediaFramePayloadFootprint::logicalBytes(
                  *receivedFrame, MediaStreamKind::Video)
            : ::media::Result<std::uint64_t>::failure(
                  ::media::ErrorInfo::invalidArgument(
                      "VideoDecodeNode wrapped frame is unavailable"));
        if (!footprint) {
            return ::media::Result<bool>::failure(footprint.error());
        }
        if (auto status = reservation.value().shrinkToActual(
                footprint.value()); !status) {
            return ::media::Result<bool>::failure(status.error());
        }
        if (auto status = reservation.value().attachTo(*buffer.value());
            !status) {
            return ::media::Result<bool>::failure(status.error());
        }

        MediaBufferRef output = buffer.value();
        if (lineage) {
            if (!codecContext() || codecContext()->pkt_timebase.num <= 0 ||
                codecContext()->pkt_timebase.den <= 0) {
                return ::media::Result<bool>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized VideoDecodeNode requires decoder packet time base"));
            }
            MediaTimeDescriptor timeDescriptor;
            timeDescriptor.timeBase = MediaRational{
                codecContext()->pkt_timebase.num,
                codecContext()->pkt_timebase.den};
            output->setTimeDescriptor(timeDescriptor);
            auto canonical = MediaCanonicalVideoFrameBuffer::create(output, std::move(lineage));
            if (!canonical) return ::media::Result<bool>::failure(canonical.error());
            output = std::move(canonical).value();
        }
        auto pushStatus = pushToMatchingOutputs(context, output, MediaStreamKind::Video);
        if (!pushStatus) {
            if (pushStatus.error().code == ::media::ErrorCode::WouldBlock &&
                !m_lineageState->flushPending) {
                m_lineageState->receivePending = true;
            }
            return ::media::Result<bool>::failure(pushStatus.error());
        }
    }
}

::media::Result<MediaNodeProcessResult> VideoDecodeNode::continueFlush(MediaGraphExecutionContext& context)
{
    if (!m_lineageState->flushSent) {
        const int sendRet = m_codecApi->sendPacket(codecContext(), nullptr);
        if (sendRet == 0 || sendRet == AVERROR_EOF) {
            m_lineageState->flushSent = true;
        }
        else if (sendRet != AVERROR(EAGAIN))
            return ::media::Result<MediaNodeProcessResult>::failure(
                FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(video flush)"));
    }
    auto drain = receiveFrames(context);
    if (!drain) return processProgress(::media::Status::failure(drain.error()));
    if (!drain.value()) return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    m_codecApi->flushBuffers(codecContext());
    if (m_lineageRegistry) {
        for (const auto generation : m_lineageState->lineageGenerations) {
            auto finished = m_lineageRegistry->finishGeneration(generation);
            if (!finished) return processProgress(std::move(finished));
        }
        m_lineageState->lineageGenerations.clear();
    }
    const bool eof = m_lineageState->flushIsEof;
    MediaBufferRef terminal = std::move(m_lineageState->flushBuffer);
    m_lineageState->flushPending = false;
    m_lineageState->flushIsEof = false;
    m_lineageState->flushSent = false;
    if (eof) {
        m_lineageState->terminals.markEof("packet");
        m_lineageState->eofEmitted = true;
    }
    if (auto freshness = m_lineageState->authorizeRetainedControl(terminal);
        !freshness) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            freshness.error());
    }
    auto status = broadcastControlToAllOutputs(context, terminal);
    return eof ? processFinished(std::move(status)) : processProgress(std::move(status));
}

} // namespace media::ffmpeg::graph
