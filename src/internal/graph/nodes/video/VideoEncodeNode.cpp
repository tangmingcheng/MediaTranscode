#include "internal/graph/nodes/video/VideoEncodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"
#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
}

#include <sstream>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status rescaleFrameForEncoder(AVFrame& frame,
                                       const MediaRational& source,
                                       AVRational target)
{
    if (!source.isKnown() || target.num <= 0 || target.den <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoEncodeNode requires explicit source and encoder time bases"));
    }
    const AVRational sourceTimeBase{source.num, source.den};
    if (frame.pts != AV_NOPTS_VALUE) {
        frame.pts = av_rescale_q(frame.pts, sourceTimeBase, target);
    }
    if (frame.pkt_dts != AV_NOPTS_VALUE) {
        frame.pkt_dts = av_rescale_q(frame.pkt_dts, sourceTimeBase, target);
    }
    if (frame.duration > 0) {
        frame.duration = av_rescale_q(frame.duration, sourceTimeBase, target);
    }
    return ::media::Status::success();
}

} // namespace

VideoEncodeLineageState::VideoEncodeLineageState(
    std::shared_ptr<MediaCodecLineageRegistry> registry,
    std::shared_ptr<MediaVideoEncoderCodecApi> codecApi) noexcept
    : MediaVideoLineageState(std::move(registry))
    , m_codecApi(std::move(codecApi))
{
}

void VideoEncodeLineageState::bindCodec(
    MediaBufferRef owner, AVCodecContext* context) noexcept
{
    m_codecOwner = std::move(owner);
    m_codecContext = context;
}

void VideoEncodeLineageState::resetCodecBinding() noexcept
{
    m_codecContext = nullptr;
    m_codecOwner.reset();
}

void VideoEncodeLineageState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    if (m_codecApi && m_codecContext) {
        m_codecApi->flushBuffers(m_codecContext);
    }
    clearLineageStorage();
}

void VideoEncodeLineageState::clearLineageStorage() noexcept
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
    pendingFrame.reset();
    pendingPayloadCredit.reset();
    pendingLineage.reset();
    lineageGenerations.clear();
    generationStartPending = true;
}

void VideoEncodeLineageState::resetForLifecycle() noexcept
{
    auto lineageLock = lock();
    clearLineageStorage();
    resetGenerationLifecycle();
    resetCodecBinding();
}
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string missingValue = {})
{
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
}

std::string pixelFormatName(int format)
{
    const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
    return name ? std::string(name) : std::string("unknown");
}

std::string codecName(const AVCodecContext* context)
{
    if (!context) {
        return "unknown";
    }
    if (context->codec && context->codec->name) {
        return context->codec->name;
    }
    const char* name = avcodec_get_name(context->codec_id);
    return name ? std::string(name) : std::string("unknown");
}

void encodeLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("video_encode.") + message);
}

::media::Status validateEncoderContextAgainstPlan(const MediaHardwareDescriptor& contract,
                                         const AVCodecContext* encoderContext,
                                         const AVFrame* frame)
{
    if (!encoderContext || !frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode requires valid encoder context and frame"));
    }

    if (contract.requiresHardwareFramesContext && !encoderContext->hw_frames_ctx) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode planner requires hardware encoder but encoder hw_frames_ctx is not set"));
    }

    if (encoderContext->pix_fmt != AV_PIX_FMT_NONE && frame->format != encoderContext->pix_fmt) {
        std::ostringstream out;
        out << "VideoEncodeNode planner requires frame format " << pixelFormatName(encoderContext->pix_fmt)
            << " but input frame format is " << pixelFormatName(frame->format);
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    return ::media::Status::success();
}

} // namespace

VideoEncodeNode::VideoEncodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoEncodeNode")
    , m_codecApi(makeMediaVideoEncoderCodecApi())
    , m_lineageState(std::make_shared<VideoEncodeLineageState>(
          nullptr, m_codecApi))
{
}

VideoEncodeNode::VideoEncodeNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoEncodeNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_codecApi(makeMediaVideoEncoderCodecApi())
    , m_lineageState(std::make_shared<VideoEncodeLineageState>(
          m_lineageRegistry, m_codecApi))
{
}

VideoEncodeNode::VideoEncodeNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry,
    std::shared_ptr<MediaVideoEncoderCodecApi> codecApi)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoEncodeNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_codecApi(std::move(codecApi))
    , m_lineageState(std::make_shared<VideoEncodeLineageState>(
          m_lineageRegistry, m_codecApi))
{
}

MediaNodeKind VideoEncodeNode::staticKind() noexcept
{
    return MediaNodeKind::VideoEncode;
}

std::string_view VideoEncodeNode::generationPurgeIdentity() noexcept
{
    return "video_encode";
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
VideoEncodeNode::generationPurgeTarget() const noexcept
{
    return m_lineageState->synchronized() ? m_lineageState : nullptr;
}

bool VideoEncodeNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto lineage = FFmpegPacketView::canonicalLineage(buffer);
    return m_lineageState->pendingOutputIsCurrent(
        buffer, lineage ? std::optional<std::uint64_t>(lineage->generation)
                        : std::nullopt);
}

::media::Status VideoEncodeNode::start(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    auto contract = MediaVideoFrameContractValidator::contractFromOptions(
        nodeOptions(context), "encoder.pipeline.input", "VideoEncodeNode");
    if (!contract) return ::media::Status::failure(contract.error());
    m_inputContract = std::move(contract).value();
    if (!parseMediaVideoEncoderAbortPolicy(
            nodeOptions(context)->value("video_encode.abort_policy"),
            m_abortPolicy)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoEncodeNode requires planner encoder abort policy"));
    }
    if (m_lineageRegistry) {
        auto forceKeyFrame = requiredBoolNodeOption(
            nodeOptions(context), "VideoEncodeNode",
            "video_encode.force_generation_start_key_frame");
        if (!forceKeyFrame) {
            return ::media::Status::failure(forceKeyFrame.error());
        }
        m_forceGenerationStartKeyFrame = forceKeyFrame.value();
        auto copyOpaque = parseMediaVideoLineageCopyOpaqueOption(
            nodeOptions(context), "video.lineage.encoder_copy_opaque");
        if (!copyOpaque) {
            return ::media::Status::failure(copyOpaque.error());
        }
        m_copyOpaqueLineage = copyOpaque.value();
    }
    return FFmpegCodecNodeRuntime::start(context);
}
void VideoEncodeNode::abort(MediaGraphExecutionContext& context) noexcept
{
    if (m_abortPolicy == MediaVideoEncoderAbortPolicy::DrainThenAbort &&
        hasCodecContext() && m_codecApi) {
        if (auto status = drainEncoderForStop(); !status) {
            mediaGraphDiagnosticLog(
                MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeLifecycle,
                "video_encode.abort_drain_failed error=" +
                    status.error().message);
        }
    }
    FFmpegCodecNodeRuntime::abort(context);
    resetRuntimeState();
}
void VideoEncodeNode::resetRuntimeState() noexcept
{
    m_encoderConfigEmitted = false;
    m_firstFrameDiagnosticEmitted = false;
    m_firstSubmitDiagnosticEmitted = false;
    m_firstPacketDiagnosticEmitted = false;
    m_sendWouldBlock.reset();
    m_forceGenerationStartKeyFrame.reset();
    m_copyOpaqueLineage.reset();
    m_abortPolicy = MediaVideoEncoderAbortPolicy::Unknown;
    m_inputContract.reset();
    m_drmPrimeFrames = 0;
    m_softwareFrames = 0;
    m_lineageState->resetForLifecycle();
}

::media::Status VideoEncodeNode::attachPendingLineage()
{
    if (!m_lineageState->pendingFrame ||
        m_lineageState->pendingFrame->opaque_ref ||
        !m_lineageState->pendingPayloadCredit) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoEncodeNode requires one unowned frame payload credit"));
    }
    ::media::Result<AVBufferRef*> opaque = m_lineageRegistry
        ? [&]() -> ::media::Result<AVBufferRef*> {
              if (!m_lineageState->pendingLineage) {
                  return ::media::Result<AVBufferRef*>::failure(
                      ::media::ErrorInfo::invalidArgument(
                          "VideoEncodeNode requires canonical frame lineage"));
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
        m_lineageState->pendingFrame->opaque_ref = opaque.value();
    } else {
        if (m_lineageState->pendingSubmissionLineage) {
            av_buffer_unref(&opaque.value());
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoEncodeNode already owns pending submission lineage"));
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

::media::Result<MediaNodeProcessResult> VideoEncodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    if (m_lineageState->flushPending) {
        return continueFlush(context);
    }
    if (m_lineageState->receivePending) {
        auto receiveResult = receivePackets(context);
        if (!receiveResult) return processProgress(::media::Status::failure(receiveResult.error()));
        m_lineageState->receivePending = false;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_lineageState->pendingFrame) return submitPendingFrame(context);
    if (m_lineageState->terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* frameInput = context.findInputChannel(nodeId(), "frame");
        if (frameInput && frameInput->closed()) {
            m_lineageState->terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const MediaBufferRef& buffer = *input.value();
    if (tryBindCodecContext(buffer)) {
        m_lineageState->bindCodec(buffer, codecContext());
        const AVCodecContext* encoder = codecContext();
        encodeLog(MediaGraphDiagnosticLevel::State,
                  std::string("bind_encoder codec=") + codecName(encoder) +
                      " pix_fmt=" + pixelFormatName(encoder ? encoder->pix_fmt : AV_PIX_FMT_NONE) +
                      " rate_control=" + optionValue(
                          nodeOptions(context),
                          MediaTranscodeOptionKey::PlannedVideoRateControl,
                          "missing") +
                      " bitrate=" + std::to_string(
                          encoder ? encoder->bit_rate : 0) +
                      " min_bitrate=" + std::to_string(
                          encoder ? encoder->rc_min_rate : 0) +
                      " max_bitrate=" + std::to_string(
                          encoder ? encoder->rc_max_rate : 0) +
                      " buffer_size=" + std::to_string(
                          encoder ? encoder->rc_buffer_size : 0) +
                      " fps=" + std::to_string(
                          encoder ? encoder->framerate.num : 0) + "/" +
                          std::to_string(
                          encoder ? encoder->framerate.den : 0) +
                      " gop=" + std::to_string(
                          encoder ? encoder->gop_size : 0) +
                      " frame_kind=" + optionValue(nodeOptions(context), "encoder.pipeline.frame_kind", "software") +
                      " hwaccel=" + optionValue(nodeOptions(context), "encoder.pipeline.hwaccel", "none") +
                      " hw_device_ctx=" + (encoder && encoder->hw_device_ctx ? "set" : "none") +
                      " hw_frames_ctx=" + (encoder && encoder->hw_frames_ctx ? "set" : "none"));
        auto emitStatus = emitEncoderConfig(context, buffer);
        if (!emitStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(emitStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    if (!hasCodecContext()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("VideoEncodeNode requires codec context before frames"));
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

    const AVFrame* frame = FFmpegFrameView::frame(buffer);
    if (!frame) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode expected frame buffer"));
    }
    const auto& inputPayloadCredit = FFmpegFrameView::payloadCredit(buffer);
    if (!inputPayloadCredit) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoEncodeNode input frame lacks payload credit ownership"));
    }

    if (!m_inputContract) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("VideoEncodeNode input frame contract is not bound"));
    }
    auto validateStatus = validateEncoderContextAgainstPlan(*m_inputContract, codecContext(), frame);
    if (!validateStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(validateStatus.error());
    }
    auto facts = MediaVideoFrameContractValidator::validate(
        *frame, *m_inputContract, "VideoEncodeNode encode input");
    if (!facts) return ::media::Result<MediaNodeProcessResult>::failure(facts.error());
    m_drmPrimeFrames += facts.value().drmPrime ? 1U : 0U;
    m_softwareFrames += facts.value().software ? 1U : 0U;
    if (!m_firstFrameDiagnosticEmitted) {
        encodeLog(MediaGraphDiagnosticLevel::State,
                  "input_contract " + MediaVideoFrameContractValidator::describe(*frame, facts.value()));
    }
    ::media::ffmpeg::FramePtr pendingFrame(av_frame_clone(frame));
    if (!pendingFrame) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed("VideoEncodeNode failed to clone input frame"));
    }
    auto rescaleStatus = rescaleFrameForEncoder(
        *pendingFrame, buffer->timeDescriptor().timeBase, codecContext()->time_base);
    if (!rescaleStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            rescaleStatus.error());
    }
    std::shared_ptr<const MediaCanonicalLineage> pendingLineage;
    if (m_lineageRegistry) {
        pendingLineage = FFmpegFrameView::canonicalLineage(buffer);
        if (!pendingLineage) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoEncodeNode requires canonical frame lineage"));
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

    m_lineageState->pendingFrame = std::move(pendingFrame);
    m_lineageState->pendingPayloadCredit = inputPayloadCredit;
    m_lineageState->pendingLineage = std::move(pendingLineage);

    return submitPendingFrame(context);
}

::media::Result<MediaNodeProcessResult> VideoEncodeNode::submitPendingFrame(
    MediaGraphExecutionContext& context)
{
    AVFrame* frame = m_lineageState->pendingFrame.get();
    if (m_lineageRegistry) {
        if (!m_forceGenerationStartKeyFrame) {
            return processProgress(::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "VideoEncodeNode requires planner generation-start key-frame policy")));
        }
        if (m_lineageState->generationStartPending &&
            *m_forceGenerationStartKeyFrame) {
            frame->pict_type = AV_PICTURE_TYPE_I;
            frame->flags |= AV_FRAME_FLAG_KEY;
            std::ostringstream out;
            out << "generation_start_key_frame pts=" << frame->pts;
            if (m_lineageState->pendingLineage) {
                out << " generation="
                    << m_lineageState->pendingLineage->generation;
            }
            encodeLog(MediaGraphDiagnosticLevel::State, out.str());
        }
    }
    if (!m_lineageState->pendingFrame->opaque_ref &&
        !m_lineageState->pendingSubmissionLineage) {
        auto attached = attachPendingLineage();
        if (!attached) return processProgress(std::move(attached));
    }

    if (!m_firstFrameDiagnosticEmitted) {
        std::ostringstream out;
        out << "first_frame pts=" << frame->pts;
        if (m_lineageState->pendingLineage) {
            out << " generation=" << m_lineageState->pendingLineage->generation;
        } else {
            out << " generation=attached";
        }
        encodeLog(MediaGraphDiagnosticLevel::State, out.str());
        m_firstFrameDiagnosticEmitted = true;
    }

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_encode.frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "frame seq=" << decision.sequence
            << " codec=" << codecName(codecContext())
            << " encoder_fmt=" << pixelFormatName(codecContext()->pix_fmt)
            << " frame_fmt=" << pixelFormatName(frame->format)
            << " frame_hw=" << (frame->hw_frames_ctx ? "set" : "none")
            << " encoder_hw_frames=" << (codecContext()->hw_frames_ctx ? "set" : "none")
            << " pts=" << frame->pts
            << " size=" << frame->width << "x" << frame->height;
        encodeLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    if (!m_codecApi) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("VideoEncodeNode codec API is missing"));
    }
    const int sendRet = m_codecApi->sendFrame(codecContext(), frame);
    const bool sendWouldBlock = sendRet == AVERROR(EAGAIN);
    if (!m_firstSubmitDiagnosticEmitted) {
        encodeLog(MediaGraphDiagnosticLevel::State,
                  std::string("first_submit result=") +
                      (sendRet == 0 ? "accepted" :
                       sendWouldBlock ? "would_block" : "failed"));
        m_firstSubmitDiagnosticEmitted = true;
    }
    if (m_sendWouldBlock && *m_sendWouldBlock != sendWouldBlock) {
        encodeLog(MediaGraphDiagnosticLevel::State,
                  std::string("submit_transition state=") +
                      (sendWouldBlock ? "would_block" : "accepted"));
    }
    m_sendWouldBlock = sendWouldBlock;
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_frame(video)"));
    }
    if (sendRet == 0) {
        if (m_lineageState->pendingSubmissionLineage) {
            m_lineageState->submissionOrderLineage.push_back(
                m_lineageState->pendingSubmissionLineage);
            m_lineageState->pendingSubmissionLineage = nullptr;
        }
        m_lineageState->pendingFrame.reset();
        m_lineageState->generationStartPending = false;
    }

    auto receiveStatus = receivePackets(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Status VideoEncodeNode::stop(MediaGraphExecutionContext& context)
{
    if (hasCodecContext()) {
        if (auto status = drainEncoderForStop(); !status) {
            return status;
        }
    }
    std::ostringstream summary;
    summary << "video_encode.zero_copy_summary drm_prime=" << m_drmPrimeFrames
            << " software_frame=" << m_softwareFrames;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            summary.str());
    auto status = FFmpegCodecNodeRuntime::stop(context);
    resetRuntimeState();
    return status;
}

::media::Status VideoEncodeNode::emitEncoderConfig(MediaGraphExecutionContext& context,
                                                   const MediaBufferRef& buffer)
{
    if (m_encoderConfigEmitted || !buffer) {
        return ::media::Status::success();
    }

    if (!context.findOutputChannel(nodeId(), "codec")) {
        m_encoderConfigEmitted = true;
        return ::media::Status::success();
    }

    auto status = emitOutput(context, "codec", buffer);
    if (!status) {
        return status;
    }

    m_encoderConfigEmitted = true;
    return ::media::Status::success();
}

::media::Result<bool> VideoEncodeNode::receivePackets(MediaGraphExecutionContext& context)
{
    while (true) {
        auto reservation = context.reservePayload(
            nodeId(), MediaStreamKind::Video, MediaPayloadKind::Packet);
        if (!reservation) {
            if (reservation.error().code == ::media::ErrorCode::WouldBlock &&
                !m_lineageState->flushPending) {
                m_lineageState->receivePending = true;
            }
            return ::media::Result<bool>::failure(reservation.error());
        }
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::allocationFailed("VideoEncodeNode failed: av_packet_alloc returned null"));
        }

        const int ret = m_codecApi->receivePacket(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN)) {
            return ::media::Result<bool>::success(false);
        }
        if (ret == AVERROR_EOF) {
            return ::media::Result<bool>::success(true);
        }

        if (ret < 0) {
            return ::media::Result<bool>::failure(
                FFmpegGraphError::fromCode(ret, "avcodec_receive_packet(video)"));
        }

        if (!m_firstPacketDiagnosticEmitted) {
            std::ostringstream out;
            out << "first_packet pts=" << packet->pts
                << " dts=" << packet->dts
                << " duration=" << packet->duration
                << " size=" << packet->size;
            encodeLog(MediaGraphDiagnosticLevel::State, out.str());
            m_firstPacketDiagnosticEmitted = true;
        }

        auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                                   "video_encode.packet");
        if (decision.shouldLog) {
            std::ostringstream out;
            out << "packet seq=" << decision.sequence
                << " codec=" << codecName(codecContext())
                << " pts=" << packet->pts
                << " dts=" << packet->dts
                << " duration=" << packet->duration
                << " size=" << packet->size;
            encodeLog(MediaGraphDiagnosticLevel::Flow, out.str());
        }

        std::shared_ptr<const MediaCanonicalLineage> lineage;
        if (m_lineageRegistry) {
            AVBufferRef* lineageOpaque = packet->opaque_ref;
            if (m_copyOpaqueLineage && !*m_copyOpaqueLineage) {
                if (m_lineageState->submissionOrderLineage.empty()) {
                    return ::media::Result<bool>::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "Submission-order encoder lineage has no pending token"));
                }
                lineageOpaque = m_lineageState->submissionOrderLineage.front();
                m_lineageState->submissionOrderLineage.pop_front();
            }
            auto resolved = m_lineageRegistry->resolveOutput(lineageOpaque);
            if (lineageOpaque != packet->opaque_ref) {
                av_buffer_unref(&lineageOpaque);
            }
            if (!resolved) return ::media::Result<bool>::failure(resolved.error());
            if (resolved.value()) lineage = std::move(*resolved.value());
            av_buffer_unref(&packet->opaque_ref);
            if (!lineage) continue;
        }

        auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), MediaStreamKind::Video, std::nullopt);
        if (!buffer) {
            return ::media::Result<bool>::failure(buffer.error());
        }
        const auto footprint = buffer.value()->payloadFootprintBytes();
        if (!footprint || *footprint == 0) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::notInitialized(
                    "VideoEncodeNode packet lacks an exact payload footprint"));
        }
        if (auto status = reservation.value().shrinkToActual(*footprint);
            !status) {
            return ::media::Result<bool>::failure(status.error());
        }
        if (auto status = reservation.value().attachTo(*buffer.value());
            !status) {
            return ::media::Result<bool>::failure(status.error());
        }

        MediaTimeDescriptor timeDescriptor;
        timeDescriptor.timeBase = MediaRational{ codecContext()->time_base.num, codecContext()->time_base.den };
        buffer.value()->setTimeDescriptor(timeDescriptor);

        MediaBufferRef output = buffer.value();
        if (lineage) {
            auto canonical = MediaCanonicalAccessUnitBuffer::create(
                output, std::move(lineage), std::nullopt);
            if (!canonical) return ::media::Result<bool>::failure(canonical.error());
            output = std::move(canonical).value();
        }
        auto emitStatus = emitOutput(context, "packet", output);
        if (!emitStatus) {
            if (emitStatus.error().code == ::media::ErrorCode::WouldBlock &&
                !m_lineageState->flushPending) {
                m_lineageState->receivePending = true;
            }
            return ::media::Result<bool>::failure(emitStatus.error());
        }
    }
}

::media::Result<MediaNodeProcessResult> VideoEncodeNode::continueFlush(
    MediaGraphExecutionContext& context)
{
    if (!m_lineageState->flushSent) {
        const int sendRet = m_codecApi->sendFrame(codecContext(), nullptr);
        if (sendRet == 0 || sendRet == AVERROR_EOF) {
            m_lineageState->flushSent = true;
        } else if (sendRet != AVERROR(EAGAIN)) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                FFmpegGraphError::fromCode(sendRet, "avcodec_send_frame(video flush)"));
        }
    }
    auto drainResult = receivePackets(context);
    if (!drainResult) {
        return processProgress(::media::Status::failure(drainResult.error()));
    }
    if (!drainResult.value()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (!m_lineageState->flushIsEof) m_codecApi->flushBuffers(codecContext());

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
        m_lineageState->terminals.markEof("frame");
        m_lineageState->eofEmitted = true;
    }
    if (auto freshness = m_lineageState->authorizeRetainedControl(terminal);
        !freshness) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            freshness.error());
    }
    auto emitStatus = emitOutput(context, "packet", terminal);
    return eof ? processFinished(std::move(emitStatus))
               : processProgress(std::move(emitStatus));
}

::media::Status VideoEncodeNode::drainEncoderForStop()
{
    const int sendRet = m_codecApi->sendFrame(codecContext(), nullptr);
    if (sendRet < 0 && sendRet != AVERROR_EOF) {
        return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(video stop)");
    }

    for (;;) {
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoEncodeNode stop failed: av_packet_alloc returned null"));
        }
        const int ret = m_codecApi->receivePacket(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }
        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "avcodec_receive_packet(video stop)");
        }
    }
}

} // namespace media::ffmpeg::graph
