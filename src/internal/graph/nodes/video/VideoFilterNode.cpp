#include "internal/graph/nodes/video/VideoFilterNode.h"

#include "internal/graph/builder/video/VideoFilterGraphBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
}

#include <array>
#include <chrono>
#include <sstream>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

VideoFilterLineageState::VideoFilterLineageState(
    std::shared_ptr<MediaCodecLineageRegistry> registry) noexcept
    : MediaVideoLineageState(std::move(registry))
{
}

void VideoFilterLineageState::resetFilterGraph() noexcept
{
    filterGraph.reset();
    bufferSrcContext = nullptr;
    bufferSinkContext = nullptr;
    inputTimeBase = AVRational{0, 1};
    sinkTimeBase = AVRational{0, 1};
    lastSubmittedPts = AV_NOPTS_VALUE;
    graphInitialized = false;
    flushed = false;
    filterEof = false;
}

void VideoFilterLineageState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    clearGenerationLineage();
}

void VideoFilterLineageState::clearGenerationLineage() noexcept
{
    terminals.reset();
    eofEmitted = false;
    terminalBuffer.reset();
    terminalPending = false;
    terminalIsEof = false;
    pendingFrame.reset();
    pendingLineage.reset();
    lineageGenerations.clear();
    lastSubmittedPts = AV_NOPTS_VALUE;
}

void VideoFilterLineageState::clearLineageStorage() noexcept
{
    resetFilterGraph();
    clearGenerationLineage();
}

void VideoFilterLineageState::resetForLifecycle() noexcept
{
    auto lineageLock = lock();
    clearLineageStorage();
    encoderConfig.reset();
    encoderContext = nullptr;
    resetGenerationLifecycle();
}
namespace {

bool rationalKnown(AVRational rational) noexcept
{
    return rational.num > 0 && rational.den > 0;
}

AVRational toAVRational(MediaRational rational) noexcept
{
    return AVRational{ rational.num, rational.den };
}

std::string rationalText(AVRational rational)
{
    if (!rationalKnown(rational)) {
        return "unknown";
    }
    return std::to_string(rational.num) + "/" + std::to_string(rational.den);
}

AVRational sanitizeSampleAspectRatio(AVRational ratio) noexcept
{
    return rationalKnown(ratio) ? ratio : AVRational{ 1, 1 };
}

bool frameRateAcceptable(AVRational frameRate) noexcept
{
    if (!rationalKnown(frameRate)) {
        return false;
    }

    const double fps = av_q2d(frameRate);
    return fps > 1.0 && fps < 240.0;
}

AVRational chooseInputFrameRate(const MediaBufferRef& buffer, AVRational plannedFrameRate) noexcept
{
    if (buffer) {
        const MediaRational frameRate = buffer->timeDescriptor().frameRate;
        if (frameRate.isKnown()) {
            const AVRational avFrameRate = toAVRational(frameRate);
            if (frameRateAcceptable(avFrameRate)) {
                return avFrameRate;
            }
        }
    }

    return frameRateAcceptable(plannedFrameRate) ? plannedFrameRate : AVRational{ 0, 1 };
}

std::string pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? std::string(name) : std::string();
}

void filterLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("video_filter.") + message);
}

} // namespace

VideoFilterNode::VideoFilterNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoFilterNode")
    , m_lineageState(std::make_shared<VideoFilterLineageState>(nullptr))
{
}

VideoFilterNode::VideoFilterNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoFilterNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_lineageState(std::make_shared<VideoFilterLineageState>(
          m_lineageRegistry))
{
}

VideoFilterNode::VideoFilterNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry,
    MediaAvStartupVideoPreparationCapability preparationCapability)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoFilterNode")
    , m_lineageRegistry(std::move(lineageRegistry))
    , m_lineageState(std::make_shared<VideoFilterLineageState>(
          m_lineageRegistry))
    , m_preparationCapability(std::move(preparationCapability))
{
}

MediaNodeKind VideoFilterNode::staticKind() noexcept
{
    return MediaNodeKind::VideoFilter;
}

std::string_view VideoFilterNode::generationPurgeIdentity() noexcept
{
    return "video_filter";
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
VideoFilterNode::generationPurgeTarget() const noexcept
{
    return m_lineageState->synchronized() ? m_lineageState : nullptr;
}

bool VideoFilterNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto lineage = FFmpegFrameView::canonicalLineage(buffer);
    return m_lineageState->pendingOutputIsCurrent(
        buffer, lineage ? std::optional<std::uint64_t>(lineage->generation)
                        : std::nullopt);
}

::media::Status VideoFilterNode::start(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    auto input = MediaVideoFrameContractValidator::contractFromOptions(
        nodeOptions(context), "filter.pipeline.input", "VideoFilterNode");
    auto output = MediaVideoFrameContractValidator::contractFromOptions(
        nodeOptions(context), "filter.pipeline.output", "VideoFilterNode");
    if (!input) return ::media::Status::failure(input.error());
    if (!output) return ::media::Status::failure(output.error());
    m_inputContract = std::move(input).value();
    m_outputContract = std::move(output).value();
    m_rgaFilter = nodeOptions(context)->value(
        "filter.pipeline.filter").starts_with("scale_rkrga=");
    return FFmpegNodeRuntime::start(context);
}
::media::Status VideoFilterNode::stop(MediaGraphExecutionContext& context)
{
    if (m_preparationCapability) m_preparationCapability->cancel();
    std::ostringstream summary;
    summary << "video_filter.zero_copy_summary drm_prime_input=" << m_drmPrimeInputFrames
            << " drm_prime_output=" << m_drmPrimeOutputFrames
            << " rga=" << m_rgaFrames
            << " software_frame=" << m_softwareFrames;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            summary.str());
    auto status = FFmpegNodeRuntime::stop(context);
    resetRuntimeState();
    return status;
}
void VideoFilterNode::abort(MediaGraphExecutionContext& context) noexcept { if (m_preparationCapability) m_preparationCapability->cancel(); FFmpegNodeRuntime::abort(context); resetRuntimeState(); }
void VideoFilterNode::resetRuntimeState() noexcept
{
    m_lineageState->resetForLifecycle();
    m_preparedOutput.reset();
    m_preparedReservation.reset();
    m_preparedGeneration = 0;
    m_preparedReleaseIdentity = 0;
    m_preparedNeedsReady = false;
    m_preparationFeedArmed = false;
    m_firstInputDiagnosticEmitted = false;
    m_firstOutputDiagnosticEmitted = false;
    m_inputContract.reset();
    m_outputContract.reset();
    m_drmPrimeInputFrames = 0;
    m_drmPrimeOutputFrames = 0;
    m_rgaFrames = 0;
    m_softwareFrames = 0;
    m_rgaFilter = false;
}

::media::Result<MediaNodeProcessResult> VideoFilterNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_preparationCapability) {
        if (m_preparedNeedsReady) {
            if (auto ready = markPreparedReadyOutsideLineageLock(context); !ready) {
                if (ready.error().code == ::media::ErrorCode::WouldBlock)
                    return processWaiting();
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ready.error());
            }
            return processWaiting();
        }
        const auto preparation = m_preparationCapability->snapshot();
        if (m_preparedReservation) {
            if (preparation.phase ==
                    MediaAvStartupVideoPreparationPhase::Cancelled) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::cancelled(
                        "VideoFilterNode preparation was cancelled"));
            }
            if (preparation.phase !=
                    MediaAvStartupVideoPreparationPhase::ReleaseCommitted) {
                return processWaiting();
            }
            if (auto committed = m_preparedReservation->commit(); !committed)
                return ::media::Result<MediaNodeProcessResult>::failure(
                    committed.error());
            m_preparedReservation.reset();
            return processProgress();
        }
        m_preparationFeedArmed = preparation.phase ==
            MediaAvStartupVideoPreparationPhase::Feeding;
        m_preparedGeneration = preparation.generation;
        m_preparedReleaseIdentity = preparation.releaseIdentity;
    }
    auto lineageLock = m_lineageState->lock();
    if (m_lineageState->terminalPending) {
        return continueTerminal(context);
    }
    bool producedPendingFrame = false;
    auto pendingDrain = drainFrames(context, &producedPendingFrame);
    if (!pendingDrain) return processProgress(std::move(pendingDrain));
    if (m_preparedNeedsReady) {
        lineageLock.unlock();
        if (auto ready = markPreparedReadyOutsideLineageLock(context); !ready) {
            if (ready.error().code == ::media::ErrorCode::WouldBlock)
                return processWaiting();
            return ::media::Result<MediaNodeProcessResult>::failure(
                ready.error());
        }
        return processWaiting();
    }
    if (producedPendingFrame) return processProgress();
    if (m_lineageState->pendingFrame) return processProgress(submitPendingFrame(context));
    if (m_lineageState->terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    if (!m_lineageState->encoderContext) {
        auto codecInput = tryPopInputOptional(context, "codec");
        if (!codecInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(codecInput.error());
        }
        if (!codecInput.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        }
        auto bindStatus = bindEncoderConfig(context, *codecInput.value());
        if (!bindStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(bindStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    auto frameInput = tryPopInputOptional(context, "frame");
    if (!frameInput) {
        return ::media::Result<MediaNodeProcessResult>::failure(frameInput.error());
    }
    if (!frameInput.value()) {
        MediaChannel* frameChannel = context.findInputChannel(nodeId(), "frame");
        if (frameChannel && frameChannel->closed()) {
            m_lineageState->terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        bool produced = false;
        auto drainStatus = drainFrames(context, &produced);
        if (!drainStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(drainStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            produced ? MediaNodeProcessResult::progress() : MediaNodeProcessResult::waiting());
    }

    MediaBufferRef frameBuffer = *frameInput.value();
    if (!m_firstInputDiagnosticEmitted) {
        filterLog(MediaGraphDiagnosticLevel::State,
                  "trace stage=first_input " +
                      mediaGraphDiagnosticDescribeBuffer(frameBuffer));
        m_firstInputDiagnosticEmitted = true;
    }
    if (frameBuffer->isEof() || frameBuffer->isFlush()) {
        const bool eof = frameBuffer->isEof();
        if (eof && m_lineageState->eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        m_lineageState->terminalBuffer = frameBuffer;
        m_lineageState->terminalPending = true;
        m_lineageState->terminalIsEof = eof;
        return continueTerminal(context);
    }

    auto sendStatus = sendFrame(context, frameBuffer);
    if (!sendStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(sendStatus.error());
    }
    if (m_preparedNeedsReady) {
        lineageLock.unlock();
        if (auto ready = markPreparedReadyOutsideLineageLock(context); !ready) {
            if (ready.error().code == ::media::ErrorCode::WouldBlock)
                return processWaiting();
            return ::media::Result<MediaNodeProcessResult>::failure(
                ready.error());
        }
        return processWaiting();
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<MediaNodeProcessResult> VideoFilterNode::continueTerminal(
    MediaGraphExecutionContext& context)
{
    auto flushStatus = flushGraph(context);
    if (!flushStatus) return processProgress(std::move(flushStatus));
    auto drainStatus = drainFrames(context);
    if (!drainStatus) return processProgress(std::move(drainStatus));
    if (!m_lineageState->filterEof) return processProgress();
    if (m_lineageRegistry) {
        for (const auto generation : m_lineageState->lineageGenerations) {
            auto finished = m_lineageRegistry->finishGeneration(generation);
            if (!finished) return processProgress(std::move(finished));
        }
        m_lineageState->lineageGenerations.clear();
    }

    const bool eof = m_lineageState->terminalIsEof;
    MediaBufferRef terminal = std::move(m_lineageState->terminalBuffer);
    m_lineageState->terminalPending = false;
    m_lineageState->terminalIsEof = false;
    if (!eof) m_lineageState->resetFilterGraph();
    if (eof) {
        m_lineageState->terminals.markEof("frame");
        m_lineageState->eofEmitted = true;
    }
    if (auto freshness = m_lineageState->authorizeRetainedControl(terminal);
        !freshness) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            freshness.error());
    }
    auto emitStatus = emitOutput(context, "frame", terminal);
    return eof ? processFinished(std::move(emitStatus))
               : processProgress(std::move(emitStatus));
}

::media::Status VideoFilterNode::bindEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codecContext = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode expected encoder codec context buffer"));
    }

    if (!rationalKnown(codecContext->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode requires encoder time_base"));
    }

    if (codecContext->pix_fmt == AV_PIX_FMT_NONE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode requires encoder pix_fmt"));
    }

    m_lineageState->encoderConfig = buffer;
    m_lineageState->encoderContext = codecContext;

    filterLog(MediaGraphDiagnosticLevel::State,
              std::string("bind_encoder codec_tb=") + rationalText(codecContext->time_base) +
                  " pix_fmt=" + pixelFormatName(codecContext->pix_fmt) +
                  " size=" + std::to_string(codecContext->width) + "x" + std::to_string(codecContext->height));

    if (context.findOutputChannel(nodeId(), "codec")) {
        return emitOutput(context, "codec", buffer);
    }

    return ::media::Status::success();
}

::media::Status VideoFilterNode::initializeGraph(MediaGraphExecutionContext& context, const MediaBufferRef& firstFrameBuffer)
{
    const AVFrame* firstFrame = FFmpegFrameView::frame(firstFrameBuffer);
    if (!firstFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode expected first frame"));
    }

    if (!m_lineageState->encoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterNode encoder context is not bound"));
    }

    const MediaRational inputTimeBase = firstFrameBuffer ? firstFrameBuffer->timeDescriptor().timeBase : MediaRational{};
    if (!inputTimeBase.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode requires input frame time_base"));
    }

    const AVRational inputFrameRate = chooseInputFrameRate(
        firstFrameBuffer, m_lineageState->encoderContext->framerate);
    if (!rationalKnown(inputFrameRate)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode cannot resolve input frame rate; upstream must provide frame rate or encoder framerate"));
    }

    const AVRational pixelAspect = sanitizeSampleAspectRatio(firstFrame->sample_aspect_ratio);

    VideoFilterGraphBuildRequest request;
    request.options = nodeOptions(context);
    request.firstFrame = firstFrame;
    request.inputTimeBase = toAVRational(inputTimeBase);
    request.inputFrameRate = inputFrameRate;
    request.sampleAspectRatio = pixelAspect;

    filterLog(MediaGraphDiagnosticLevel::State,
              "trace stage=initialize_begin input_fmt=" +
                  pixelFormatName(static_cast<AVPixelFormat>(firstFrame->format)) +
                  " input_size=" + std::to_string(firstFrame->width) + "x" +
                  std::to_string(firstFrame->height));
    const auto initializeStartedAt = std::chrono::steady_clock::now();
    auto graphResult = VideoFilterGraphBuilder::build(request);
    const auto initializeElapsed = std::chrono::duration_cast<
        std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - initializeStartedAt);
    if (!graphResult) {
        m_lineageState->resetFilterGraph();
        return ::media::Status::failure(graphResult.error());
    }

    VideoFilterGraphBuildResult built = std::move(graphResult).value();
    m_lineageState->resetFilterGraph();
    m_lineageState->filterGraph = std::move(built.graph);
    m_lineageState->bufferSrcContext = built.bufferSource;
    m_lineageState->bufferSinkContext = built.bufferSink;
    m_lineageState->inputTimeBase = request.inputTimeBase;
    m_lineageState->sinkTimeBase = built.sinkTimeBase;
    m_lineageState->graphInitialized = true;
    m_lineageState->flushed = false;

    const AVPixelFormat inputFormat = static_cast<AVPixelFormat>(firstFrame->format);
    std::ostringstream out;
    out << "initialize input_tb=" << rationalText(m_lineageState->inputTimeBase)
        << " input_fps=" << rationalText(inputFrameRate)
        << " sink_tb=" << rationalText(m_lineageState->sinkTimeBase)
        << " encoder_tb=" << rationalText(m_lineageState->encoderContext->time_base)
        << " input_fmt=" << pixelFormatName(inputFormat)
        << " encoder_fmt=" << pixelFormatName(m_lineageState->encoderContext->pix_fmt)
        << " input_size=" << firstFrame->width << "x" << firstFrame->height
        << " encoder_size=" << m_lineageState->encoderContext->width << "x"
        << m_lineageState->encoderContext->height
        << " hardware_source=" << (built.hardwareSource ? "true" : "false")
        << " planner_filter=" << built.plannerFilter
        << " desc=" << built.filterDescription
        << " initialize_elapsed_ms=" << initializeElapsed.count();
    filterLog(MediaGraphDiagnosticLevel::State, out.str());

    return ::media::Status::success();
}

::media::Status VideoFilterNode::sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    std::shared_ptr<const MediaCanonicalLineage> pendingLineage;
    if (m_lineageRegistry) {
        pendingLineage = FFmpegFrameView::canonicalLineage(buffer);
        if (!pendingLineage) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoFilterNode requires canonical frame lineage"));
        }
        auto disposition = m_lineageState->classifyObservation(
            pendingLineage->generation);
        if (!disposition) {
            return ::media::Status::failure(disposition.error());
        }
        if (disposition.value() ==
            MediaVideoLineageGenerationDisposition::DropStale) {
            return ::media::Status::success();
        }
        if (auto status = m_lineageState->validateObservation(
                pendingLineage->generation); !status) {
            return status;
        }
    }
    if (!m_lineageState->graphInitialized) {
        auto initStatus = initializeGraph(context, buffer);
        if (!initStatus) {
            return initStatus;
        }
    }

    const AVFrame* frame = FFmpegFrameView::frame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode expected frame buffer"));
    }
    if (!m_inputContract) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterNode input frame contract is not bound"));
    }
    auto inputFacts = MediaVideoFrameContractValidator::validate(
        *frame, *m_inputContract, "VideoFilterNode filter input");
    if (!inputFacts) return ::media::Status::failure(inputFacts.error());
    m_drmPrimeInputFrames += inputFacts.value().drmPrime ? 1U : 0U;
    m_softwareFrames += inputFacts.value().software ? 1U : 0U;
    if (!m_firstInputDiagnosticEmitted) {
        filterLog(MediaGraphDiagnosticLevel::State,
                  "input_contract " + MediaVideoFrameContractValidator::describe(*frame, inputFacts.value()));
    }

    ::media::ffmpeg::FramePtr pendingFrame(av_frame_clone(frame));
    if (!pendingFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode failed to clone input frame"));
    }
    if (m_lineageRegistry) {
        if (auto status = m_lineageState->observe(pendingLineage->generation);
            !status) {
            return status;
        }
    }
    m_lineageState->pendingFrame = std::move(pendingFrame);
    m_lineageState->pendingLineage = std::move(pendingLineage);
    return submitPendingFrame(context);
}

::media::Status VideoFilterNode::attachPendingLineage()
{
    if (!m_lineageRegistry) return ::media::Status::success();
    if (!m_lineageState->pendingFrame || !m_lineageState->pendingLineage ||
        m_lineageState->pendingFrame->opaque_ref) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoFilterNode requires one unowned pending frame lineage"));
    }
    auto token = m_lineageRegistry->submit(m_lineageState->pendingLineage);
    if (!token) return ::media::Status::failure(token.error());
    auto opaque = makeMediaFfmpegLineageOpaque(std::move(token).value());
    if (!opaque) return ::media::Status::failure(opaque.error());
    m_lineageState->pendingFrame->opaque_ref = opaque.value();
    m_lineageState->lineageGenerations.insert(
        m_lineageState->pendingLineage->generation);
    m_lineageState->pendingLineage.reset();
    return ::media::Status::success();
}

::media::Status VideoFilterNode::submitPendingFrame(
    MediaGraphExecutionContext& context)
{
    if (m_lineageRegistry && !m_lineageState->pendingFrame->opaque_ref) {
        auto attached = attachPendingLineage();
        if (!attached) return attached;
    }

    const int ret = av_buffersrc_add_frame_flags(m_lineageState->bufferSrcContext,
                                                 m_lineageState->pendingFrame.get(),
                                                 AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_buffersrc_add_frame_flags(video)");
    }
    m_lineageState->pendingFrame.reset();

    return drainFrames(context);
}

::media::Status VideoFilterNode::flushGraph(MediaGraphExecutionContext& context)
{
    if (!m_lineageState->graphInitialized || m_lineageState->flushed) {
        return ::media::Status::success();
    }

    const int ret = av_buffersrc_add_frame_flags(
        m_lineageState->bufferSrcContext, nullptr, 0);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_buffersrc_add_frame_flags(video flush)");
    }

    m_lineageState->flushed = true;
    return drainFrames(context);
}

::media::Status VideoFilterNode::drainFrames(MediaGraphExecutionContext& context, bool* produced)
{
    if (produced) *produced = false;
    if (!m_lineageState->graphInitialized) {
        return ::media::Status::success();
    }

    while (true) {
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: av_frame_alloc returned null"));
        }

        const int ret = av_buffersink_get_frame(
            m_lineageState->bufferSinkContext, frame.get());
        if (ret == AVERROR(EAGAIN)) {
            return ::media::Status::success();
        }
        if (ret == AVERROR_EOF) {
            m_lineageState->filterEof = true;
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "av_buffersink_get_frame(video)");
        }

        auto rescaleStatus = rescaleAndValidateFrame(frame.get());
        if (!rescaleStatus) {
            return rescaleStatus;
        }

        auto emitStatus = emitFrame(context, std::move(frame));
        if (!emitStatus) {
            return emitStatus;
        }
        if (produced) *produced = true;
        if (m_preparedOutput) return ::media::Status::success();
    }
}

::media::Status VideoFilterNode::emitFrame(MediaGraphExecutionContext& context, ::media::ffmpeg::FramePtr frame)
{
    if (!m_outputContract) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterNode output frame contract is not bound"));
    }
    auto outputFacts = MediaVideoFrameContractValidator::validate(
        *frame, *m_outputContract, "VideoFilterNode filter output");
    if (!outputFacts) return ::media::Status::failure(outputFacts.error());
    m_drmPrimeOutputFrames += outputFacts.value().drmPrime ? 1U : 0U;
    m_rgaFrames += m_rgaFilter ? 1U : 0U;
    m_softwareFrames += outputFacts.value().software ? 1U : 0U;
    std::shared_ptr<const MediaCanonicalLineage> lineage;
    if (m_lineageRegistry) {
        auto resolved = m_lineageRegistry->resolveOutput(frame->opaque_ref);
        if (!resolved) return ::media::Status::failure(resolved.error());
        if (resolved.value()) lineage = std::move(*resolved.value());
        av_buffer_unref(&frame->opaque_ref);
        if (!lineage) return ::media::Status::success();
    }
    auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = MediaRational{
        m_lineageState->encoderContext->time_base.num,
        m_lineageState->encoderContext->time_base.den};
    buffer.value()->setTimeDescriptor(timeDescriptor);

    AVFrame* outputFrame = FFmpegFrameView::writableFrame(buffer.value());
    if (outputFrame) {
        buffer.value()->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);
    }

    MediaBufferRef output = buffer.value();
    if (lineage) {
        auto canonical = MediaCanonicalVideoFrameBuffer::create(output, std::move(lineage));
        if (!canonical) return ::media::Status::failure(canonical.error());
        output = std::move(canonical).value();
    }
    if (m_preparationCapability && m_preparationFeedArmed &&
        !m_preparedOutput) {
        const auto outputLineage = FFmpegFrameView::canonicalLineage(output);
        if (!outputLineage ||
            outputLineage->generation != m_preparedGeneration) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoFilterNode rejects preparation generation mismatch"));
        }
        m_preparedOutput = output;
        m_preparedNeedsReady = true;
        return ::media::Status::success();
    }
    auto status = emitOutput(context, "frame", output);
    if (status && !m_firstOutputDiagnosticEmitted) {
        filterLog(MediaGraphDiagnosticLevel::State,
                  "trace stage=first_output " +
                      MediaVideoFrameContractValidator::describe(*outputFrame, outputFacts.value()));
        m_firstOutputDiagnosticEmitted = true;
    }
    return status;
}

::media::Status VideoFilterNode::retainPreparedOutput(
    MediaBufferRef output,
    std::uint64_t generation,
    std::uint64_t releaseIdentity)
{
    if (!m_preparationCapability || !output || m_preparedOutput) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode cannot retain prepared output"));
    }
    m_preparedOutput = std::move(output);
    m_preparedGeneration = generation;
    m_preparedReleaseIdentity = releaseIdentity;
    m_preparedNeedsReady = true;
    return ::media::Status::success();
}

::media::Status VideoFilterNode::markPreparedReadyOutsideLineageLock(
    MediaGraphExecutionContext& context)
{
    if (!m_preparationCapability || !m_preparedOutput ||
        !m_preparedNeedsReady) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode has no prepared output readiness to publish"));
    }
    const std::array<MediaAtomicOutputBatch, 1> batches{
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "frame"),
            std::span(&m_preparedOutput, 1)}};
    auto reservation = MediaReservedOutputTransaction::reserve(
        "VideoFilterNode prepared frame", batches);
    if (!reservation) return ::media::Status::failure(reservation.error());
    if (!reservation.value()) {
        return ::media::Status::failure(::media::ErrorInfo::wouldBlock(
            "VideoFilterNode prepared output is full"));
    }
    m_preparedReservation.emplace(std::move(*reservation.value()));
    m_preparedOutput.reset();
    auto ready = m_preparationCapability->markFilterReady(
        m_preparedGeneration, m_preparedReleaseIdentity,
        m_preparedReservation->handle());
    if (ready) {
        m_preparedNeedsReady = false;
    } else {
        m_preparedReservation.reset();
    }
    return ready;
}

::media::Status VideoFilterNode::rescaleAndValidateFrame(AVFrame* frame) noexcept
{
    if (!frame || !m_lineageState->encoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode filtered frame is invalid"));
    }

    if (frame->pts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode filtered frame has invalid pts"));
    }

    const int64_t ptsIn = frame->pts;
    auto rescaledPts = rescaleStrictlyIncreasingTimestamp(frame->pts,
                                                          m_lineageState->sinkTimeBase,
                                                          m_lineageState->encoderContext->time_base,
                                                          m_lineageState->lastSubmittedPts);
    if (!rescaledPts) {
        return ::media::Status::failure(rescaledPts.error());
    }

    frame->pts = rescaledPts.value();

    m_lineageState->lastSubmittedPts = frame->pts;

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_filter.frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "frame seq=" << decision.sequence
            << " sink_tb=" << rationalText(m_lineageState->sinkTimeBase)
            << " encoder_tb=" << rationalText(
                   m_lineageState->encoderContext->time_base)
            << " pts_in=" << ptsIn
            << " pts_out=" << frame->pts
            << " fmt=" << pixelFormatName(static_cast<AVPixelFormat>(frame->format))
            << " duration=" << frame->duration;
        filterLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
