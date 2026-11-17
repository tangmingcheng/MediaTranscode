#include "internal/graph/nodes/video/MediaVideoFrameReadinessBinding.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

#include "internal/graph/builder/video/VideoFilterGraphBuilder.h"
#include "internal/graph/planner/capability/MediaVideoSourceIsolationAdapter.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"
#include "internal/graph/nodes/video/MediaVideoFilterExecutionPlanCodec.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"
#include "internal/graph/runtime/ffmpeg/MediaFfmpegPayloadOwnership.h"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
#include <libavutil/mathematics.h>
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
    if (generationDrained || terminalPending) resetFilterGraph();
    clearGenerationLineage();
}

void VideoFilterLineageState::clearGenerationLineage() noexcept
{
    terminals.reset();
    eofEmitted = false;
    terminalBuffer.reset();
    terminalPending = false;
    terminalIsEof = false;
    generationDrained = false;
    pendingObservation.reset();
    pendingFrame.reset();
    pendingInputAllocations.reset();
    acceptedInputOutputCount.reset();
    pendingPayloadCredit.reset();
    pendingLineage.reset();
    lastDisplaySequence.reset();
    lastDisplayPresentation.reset();
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
    outputTimeBase = AVRational{0, 1};
    inputFrameRate = AVRational{0, 1};
    sourceFramesOwner.reset();
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
    std::optional<std::uint64_t> generation;
    if (const auto display = FFmpegFrameView::displayLineage(buffer))
        generation = mediaCodecLineageGeneration(MediaCodecLineage{display});
    else if (const auto canonical = FFmpegFrameView::canonicalLineage(buffer))
        generation = canonical->generation;
    return m_lineageState->pendingOutputIsCurrent(buffer, generation);
}

::media::Status VideoFilterNode::start(MediaGraphExecutionContext& context)
{
    freezeSourceReferenceResource();
    resetRuntimeState();
    const auto* options = nodeOptions(context);
    if (!options) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "VideoFilterNode requires planned execution options"));
    auto execution = MediaVideoFilterExecutionPlanCodec::decode(*options);
    if (!execution) return ::media::Status::failure(execution.error());
    if (execution.value().output.filterDescription != options->value("filter.pipeline.filter"))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode filter description differs from its execution authority"));
    if (execution.value().frameProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint &&
        !m_lineageRegistry) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode display points require a planned lineage registry"));
    m_executionPlan = std::move(execution).value();
    if (m_executionPlan->frameProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint &&
        !sourceReferenceResource())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Display filter requires its exact source allocation owner"));
    auto input = MediaVideoFrameContractValidator::contractFromOptions(
        nodeOptions(context), "filter.pipeline.input", "VideoFilterNode");
    auto output = MediaVideoFrameContractValidator::contractFromOptions(
        nodeOptions(context), "filter.pipeline.output", "VideoFilterNode");
    if (!input) return ::media::Status::failure(input.error());
    if (!output) return ::media::Status::failure(output.error());
    m_inputContract = std::move(input).value();
    m_outputContract = std::move(output).value();
    if (!parseMediaVideoFilterImplementation(
            nodeOptions(context)->value("filter.pipeline.implementation"),
            m_filterImplementation) ||
        m_filterImplementation == MediaVideoFilterImplementation::None) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoFilterNode requires planner filter implementation"));
    }
    return FFmpegNodeRuntime::start(context);
}
::media::Status VideoFilterNode::stop(MediaGraphExecutionContext& context)
{
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
void VideoFilterNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegNodeRuntime::abort(context); resetRuntimeState(); }
void VideoFilterNode::resetRuntimeState() noexcept
{
    m_lineageState->resetForLifecycle();
    m_firstInputDiagnosticEmitted = false;
    m_firstOutputDiagnosticEmitted = false;
    m_executionPlan.reset();
    m_inputContract.reset();
    m_outputContract.reset();
    m_drmPrimeInputFrames = 0;
    m_drmPrimeOutputFrames = 0;
    m_rgaFrames = 0;
    m_softwareFrames = 0;
    m_filterImplementation = MediaVideoFilterImplementation::Unknown;
}

::media::Result<MediaNodeProcessResult> VideoFilterNode::process(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaNodeProcessResult> VideoFilterNode::onProcess(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    if (m_lineageState->terminalPending) {
        return continueTerminal(context);
    }
    if (m_lineageState->generationDrained) return processWaiting();
    bool producedPendingFrame = false;
    auto pendingDrain = drainFrames(context, &producedPendingFrame);
    if (!pendingDrain) return processProgress(std::move(pendingDrain));
    if (producedPendingFrame) return processProgress();
    if (m_lineageState->pendingFrame) return processProgress(submitPendingFrame(context));
    if (m_lineageState->terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    if (m_executionPlan->timing == MediaVideoFilterTimingAuthority::PreparedEncoderMetadata &&
        !rationalKnown(m_lineageState->outputTimeBase)) {
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
    auto generationEnd = mediaEndGeneration(frameBuffer);
    if (!generationEnd) return processProgress(::media::Status::failure(generationEnd.error()));
    if (frameBuffer->isEof() || frameBuffer->isFlush() || generationEnd.value()) {
        if (generationEnd.value()) {
            if (!m_lineageState->synchronized()) return processProgress(::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Video filter generation end requires a synchronized purge owner")));
            if (auto status = m_lineageState->authorizeRetainedControl(frameBuffer); !status)
                return processProgress(status);
        }
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
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<MediaNodeProcessResult> VideoFilterNode::continueTerminal(
    MediaGraphExecutionContext& context)
{
    auto flushStatus = flushGraph(context);
    if (!flushStatus) return processProgress(std::move(flushStatus));
    auto drainStatus = drainFrames(context);
    if (!drainStatus) return processProgress(std::move(drainStatus));
    if (m_lineageState->graphInitialized && !m_lineageState->filterEof)
        return processProgress();
    if (!m_lineageState->graphInitialized &&
        (m_lineageState->pendingFrame || m_lineageState->pendingLineage ||
         m_lineageState->pendingPayloadCredit || m_lineageState->pendingInputAllocations ||
         !m_lineageState->lineageGenerations.empty()))
        return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video filter without a graph retains undrained generation data")));
    if (m_lineageRegistry) {
        for (const auto generation : m_lineageState->lineageGenerations) {
            auto finished = m_lineageRegistry->finishGeneration(generation);
            if (!finished) return processProgress(std::move(finished));
        }
        m_lineageState->lineageGenerations.clear();
    }

    const bool eof = m_lineageState->terminalIsEof;
    MediaBufferRef terminal = std::move(m_lineageState->terminalBuffer);
    auto generationEnd = mediaEndGeneration(terminal);
    if (!generationEnd) return processProgress(::media::Status::failure(generationEnd.error()));
    m_lineageState->generationDrained = generationEnd.value().has_value();
    m_lineageState->terminalPending = false;
    m_lineageState->terminalIsEof = false;
    if (!eof && !generationEnd.value()) m_lineageState->resetFilterGraph();
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

    auto snapshot = FFmpegCodecParametersMaterializer::snapshot(*codecContext);
    if (!snapshot) return ::media::Status::failure(snapshot.error());
    const auto& time = snapshot.value()->timeDescriptor();
    if (!time.timeBase.isKnown() || !time.frameRate.isKnown())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode requires immutable prepared encoder time base and frame rate"));
    m_lineageState->outputTimeBase = toAVRational(time.timeBase);
    m_lineageState->inputFrameRate =
        m_executionPlan->frameProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint
            ? AVRational{0, 1} : toAVRational(time.frameRate);

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

    if (!m_executionPlan) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "VideoFilterNode execution plan is not bound"));
    const MediaRational inputTimeBase = firstFrameBuffer->timeDescriptor().timeBase;
    if (!inputTimeBase.isKnown()) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "VideoFilterNode requires authoritative input frame time base"));
    AVRational pixelAspect = firstFrame->sample_aspect_ratio;
    if (m_executionPlan->timing == MediaVideoFilterTimingAuthority::SourceFrame) {
        m_lineageState->outputTimeBase = toAVRational(inputTimeBase);
        m_lineageState->inputFrameRate = toAVRational(m_executionPlan->sourceFrameRate);
        const auto preparedAspect = toAVRational(m_executionPlan->sourceSampleAspectRatio);
        if (rationalKnown(pixelAspect) && rationalKnown(preparedAspect) &&
            av_cmp_q(pixelAspect, preparedAspect) != 0)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoFilterNode source aspect ratio differs from prepared source facts"));
    }
    if (m_executionPlan->output.allocation == MediaVideoFilterAllocation::IndependentOutputPool) {
        if (!firstFrame->hw_frames_ctx) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode independent output pool requires a hardware source pool"));
        m_lineageState->sourceFramesOwner = ::media::ffmpeg::makeBufferRef(firstFrame->hw_frames_ctx);
        if (!m_lineageState->sourceFramesOwner) return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("VideoFilterNode source pool identity"));
    }
    const AVRational inputFrameRate = m_lineageState->inputFrameRate;
    const bool displayPoints = m_executionPlan->frameProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint;
    if ((!displayPoints && !rationalKnown(inputFrameRate)) || !rationalKnown(m_lineageState->outputTimeBase) ||
        pixelAspect.num < 0 || pixelAspect.den <= 0) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode requires explicit frame rate, output time base and source sample aspect ratio"));

    VideoFilterGraphBuildRequest request;
    request.options = nodeOptions(context);
    request.firstFrame = firstFrame;
    request.inputTimeBase = toAVRational(inputTimeBase);
    if (!displayPoints) request.inputFrameRate = inputFrameRate;
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
    if (m_executionPlan->output.allocation == MediaVideoFilterAllocation::IndependentOutputPool) {
        auto actual = MediaVideoSourceIsolationAdapter::inspect(*built.graph, m_executionPlan->output.filterDescription);
        if (!actual) return ::media::Status::failure(actual.error());
        if (actual.value() != m_executionPlan->output)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoFilterNode implementation differs from its independent output evidence"));
    }
    m_lineageState->resetFilterGraph();
    m_lineageState->filterGraph = std::move(built.graph);
    m_lineageState->bufferSrcContext = built.bufferSource;
    m_lineageState->bufferSinkContext = built.bufferSink;
    m_lineageState->inputTimeBase = request.inputTimeBase;
    m_lineageState->sinkTimeBase = built.sinkTimeBase;
    if (displayPoints) m_lineageState->outputTimeBase = built.sinkTimeBase;
    m_lineageState->graphInitialized = true;
    m_lineageState->flushed = false;

    const AVPixelFormat inputFormat = static_cast<AVPixelFormat>(firstFrame->format);
    std::ostringstream out;
    out << "initialize input_tb=" << rationalText(m_lineageState->inputTimeBase)
        << " input_fps=" << rationalText(inputFrameRate)
        << " sink_tb=" << rationalText(m_lineageState->sinkTimeBase)
        << " output_tb=" << rationalText(m_lineageState->outputTimeBase)
        << " input_fmt=" << pixelFormatName(inputFormat)
        << " input_size=" << firstFrame->width << "x" << firstFrame->height
        << " hardware_source=" << (built.hardwareSource ? "true" : "false")
        << " planner_filter=" << built.plannerFilter
        << " desc=" << built.filterDescription
        << " allocation_authority=" << m_executionPlan->output.allocationAuthority
        << " completion_authority=" << m_executionPlan->output.completionAuthority
        << " initialize_elapsed_ms=" << initializeElapsed.count();
    filterLog(MediaGraphDiagnosticLevel::State, out.str());

    return ::media::Status::success();
}

::media::Status VideoFilterNode::sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    std::optional<MediaCodecLineage> pendingLineage;
    if (m_lineageRegistry) {
        if (m_executionPlan->frameProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint)
            pendingLineage = MediaCodecLineage{FFmpegFrameView::displayLineage(buffer)};
        else
            pendingLineage = MediaCodecLineage{FFmpegFrameView::canonicalLineage(buffer)};
        if (auto status = validateMediaCodecLineage(*pendingLineage); !status) return status;
        auto disposition = m_lineageState->classifyObservation(
            mediaCodecLineageGeneration(*pendingLineage));
        if (!disposition) {
            return ::media::Status::failure(disposition.error());
        }
        if (disposition.value() ==
            MediaVideoLineageGenerationDisposition::DropStale) {
            m_sourceVideoObservation.droppedStale();
            return ::media::Status::success();
        }
        if (auto status = m_lineageState->validateObservation(
                mediaCodecLineageGeneration(*pendingLineage)); !status) {
            return status;
        }
    }
    if (!m_lineageState->graphInitialized) {
        auto initStatus = initializeGraph(context, buffer);
        if (!initStatus) {
            return initStatus;
        }
    }

    if (auto ready = MediaVideoFrameReadinessBinding::admitInput(buffer, nodeOptions(context)); !ready) return ready;
    const AVFrame* frame = FFmpegFrameView::frame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode expected frame buffer"));
    }
    if (m_executionPlan->output.allocation == MediaVideoFilterAllocation::IndependentOutputPool &&
        (!frame->hw_frames_ctx || !m_lineageState->sourceFramesOwner ||
         frame->hw_frames_ctx->data != m_lineageState->sourceFramesOwner->data))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode source pool changed after independent output negotiation"));
    const auto& inputPayloadCredit = FFmpegFrameView::payloadCredit(buffer);
    if (!inputPayloadCredit && context.payloadCreditsRequired()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "VideoFilterNode input frame lacks payload credit ownership"));
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
        if (auto status = m_lineageState->observe(mediaCodecLineageGeneration(*pendingLineage));
            !status) {
            return status;
        }
    }
    m_lineageState->pendingObservation = mediaSourceVideoObservationFacts(buffer);
    if (m_lineageState->pendingObservation)
        m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::Input, *m_lineageState->pendingObservation);
    m_lineageState->pendingFrame = std::move(pendingFrame);
    m_lineageState->pendingPayloadCredit = inputPayloadCredit;
    m_lineageState->pendingLineage = std::move(pendingLineage);
    return submitPendingFrame(context);
}

::media::Status VideoFilterNode::attachPendingLineage()
{
    if (!m_lineageState->pendingFrame ||
        m_lineageState->pendingFrame->opaque_ref ||
        (!m_lineageState->pendingPayloadCredit && !m_lineageRegistry)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoFilterNode requires one unowned frame payload credit"));
    }
    if (m_lineageState->pendingPayloadCredit) {
        auto retained = retainMediaFfmpegPayload(*m_lineageState->pendingFrame,
                                                {m_lineageState->pendingPayloadCredit, nullptr, MediaRuntimeLineageAllocation{}});
        if (!retained) return retained;
        m_lineageState->pendingPayloadCredit.reset();
    }
    if (!m_lineageRegistry) return ::media::Status::success();
    if (!m_lineageState->pendingLineage) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode requires planned input lineage"));
    }
    auto token = m_lineageRegistry->submit(*m_lineageState->pendingLineage);
    if (!token) return ::media::Status::failure(token.error());
    auto opaque = makeMediaFfmpegCodecOpaque(std::move(token).value());
    if (!opaque) return ::media::Status::failure(opaque.error());
    m_lineageState->pendingFrame->opaque_ref = opaque.value();
    if (m_lineageState->pendingLineage) {
        m_lineageState->lineageGenerations.insert(
            mediaCodecLineageGeneration(*m_lineageState->pendingLineage));
    }
    m_lineageState->pendingLineage.reset();
    m_lineageState->pendingPayloadCredit.reset();
    return ::media::Status::success();
}

::media::Status VideoFilterNode::submitPendingFrame(
    MediaGraphExecutionContext& context)
{
    if (!m_lineageState->pendingFrame || m_lineageState->acceptedInputOutputCount) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode requires an unsubmitted input and no unfinished immediate consumption"));
    }
    if ((m_lineageRegistry || m_lineageState->pendingPayloadCredit) &&
        !m_lineageState->pendingFrame->opaque_ref) {
        auto attached = attachPendingLineage();
        if (!attached) return attached;
    }

    if (m_executionPlan->inputConsumption) {
        auto references = MediaFfmpegPayloadReferences::capture(*m_lineageState->pendingFrame, sourceReferenceResource());
        if (!references) return ::media::Status::failure(references.error());
        m_lineageState->pendingInputAllocations = std::move(references).value();
    }
    auto failure = MediaVideoFrameReadinessBinding::guard(nodeOptions(context));
    if (!failure) return ::media::Status::failure(failure.error());
    const int ret = av_buffersrc_add_frame_flags(m_lineageState->bufferSrcContext,
                                                 m_lineageState->pendingFrame.get(),
                                                 AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) && m_lineageState->pendingObservation)
            m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::WouldBlock, *m_lineageState->pendingObservation);
        return FFmpegGraphError::statusFromCode(ret, "av_buffersrc_add_frame_flags(video)");
    }
    if (m_lineageState->pendingObservation)
        m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::Accepted, *m_lineageState->pendingObservation);
    m_lineageState->pendingObservation.reset();
    // KEEP_REF leaves our AVFrame intact, but the filter already accepted it.
    // Output pressure can only resume draining; it must never resubmit input.
    m_lineageState->pendingFrame.reset();
    if (m_executionPlan->inputConsumption)
        m_lineageState->acceptedInputOutputCount = 0;
    auto drained = drainFrames(context);
    if (!drained) return drained;
    failure.value().release();
    return drained;
}

::media::Status VideoFilterNode::flushGraph(MediaGraphExecutionContext& context)
{
    if (!m_lineageState->graphInitialized || m_lineageState->flushed) {
        return ::media::Status::success();
    }

    auto failure = MediaVideoFrameReadinessBinding::guard(nodeOptions(context));
    if (!failure) return ::media::Status::failure(failure.error());
    const int ret = av_buffersrc_add_frame_flags(
        m_lineageState->bufferSrcContext, nullptr, 0);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_buffersrc_add_frame_flags(video flush)");
    }

    m_lineageState->flushed = true;
    auto drained = drainFrames(context);
    if (drained) failure.value().release();
    return drained;
}

::media::Status VideoFilterNode::drainFrames(MediaGraphExecutionContext& context, bool* produced)
{
    if (produced) *produced = false;
    if (!m_lineageState->graphInitialized) {
        return ::media::Status::success();
    }

    while (true) {
        auto reservation = context.reservePayload(
            nodeId(), MediaStreamKind::Video, MediaPayloadKind::Frame);
        if (!reservation) {
            return ::media::Status::failure(reservation.error());
        }
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoFilterNode failed: av_frame_alloc returned null"));
        }

        auto failure = MediaVideoFrameReadinessBinding::guard(nodeOptions(context));
        if (!failure) return ::media::Status::failure(failure.error());
        const int ret = av_buffersink_get_frame(
            m_lineageState->bufferSinkContext, frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            if (m_lineageState->acceptedInputOutputCount) {
                if (*m_lineageState->acceptedInputOutputCount != 1)
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "Immediate filter consumption requires exactly one output per accepted input"));
                m_lineageState->acceptedInputOutputCount.reset();
                m_lineageState->pendingInputAllocations.reset();
            }
            failure.value().release();
            if (ret == AVERROR_EOF) m_lineageState->filterEof = true;
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "av_buffersink_get_frame(video)");
        }

        if (m_executionPlan->inputConsumption) {
            if (!m_lineageState->acceptedInputOutputCount ||
                *m_lineageState->acceptedInputOutputCount != 0 ||
                !m_lineageState->pendingInputAllocations)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Immediate filter produced output outside its admitted single-input consumption"));
            ++*m_lineageState->acceptedInputOutputCount;
        }
        auto rescaleStatus = rescaleAndValidateFrame(frame.get());
        if (!rescaleStatus) {
            return rescaleStatus;
        }

        failure.value().release(); // emitFrame owns the moved frame and failure scope.
        auto emitStatus = emitFrame(
            context, std::move(frame), std::move(reservation).value());
        if (!emitStatus) {
            return emitStatus;
        }
        if (produced) *produced = true;
    }
}

::media::Status VideoFilterNode::emitFrame(
    MediaGraphExecutionContext& context,
    ::media::ffmpeg::FramePtr frame,
    MediaGraphPayloadReservation reservation)
{
    auto failure = MediaVideoFrameReadinessBinding::guard(nodeOptions(context));
    if (!failure) return ::media::Status::failure(failure.error());
    if (!m_outputContract) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFilterNode output frame contract is not bound"));
    }
    if (m_executionPlan->output.allocation == MediaVideoFilterAllocation::IndependentOutputPool &&
        (!frame->hw_frames_ctx || !m_lineageState->sourceFramesOwner ||
         frame->hw_frames_ctx->data == m_lineageState->sourceFramesOwner->data))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoFilterNode output aliases the source pool despite its independent allocation contract"));
    auto outputFacts = MediaVideoFrameContractValidator::validate(
        *frame, *m_outputContract, "VideoFilterNode filter output");
    if (!outputFacts) return ::media::Status::failure(outputFacts.error());
    m_drmPrimeOutputFrames += outputFacts.value().drmPrime ? 1U : 0U;
    m_rgaFrames += m_filterImplementation == MediaVideoFilterImplementation::Rga
        ? 1U : 0U;
    m_softwareFrames += outputFacts.value().software ? 1U : 0U;
    std::shared_ptr<const MediaCanonicalLineage> lineage;
    std::shared_ptr<const MediaVideoDisplayLineage> display;
    if (m_lineageRegistry) {
        if (m_executionPlan->frameProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint) {
            auto resolved = m_lineageRegistry->resolveOutput<MediaVideoDisplayLineage>(frame->opaque_ref, nullptr);
            if (!resolved) { m_sourceVideoObservation.unresolved(); return ::media::Status::failure(resolved.error()); }
            if (resolved.value()) display = std::move(*resolved.value());
        } else {
            auto resolved = m_lineageRegistry->resolveOutput<MediaCanonicalLineage>(frame->opaque_ref, nullptr);
            if (!resolved) { m_sourceVideoObservation.unresolved(); return ::media::Status::failure(resolved.error()); }
            if (resolved.value()) lineage = std::move(*resolved.value());
        }
        av_buffer_unref(&frame->opaque_ref);
        if (!lineage && !display) { m_sourceVideoObservation.unresolved(); return ::media::Status::success(); }
    }
    if (display) {
        if (av_compare_ts(frame->pts, m_lineageState->sinkTimeBase,
                display->decodedPresentation, toAVRational(display->decodedTimeBase)) != 0 ||
            (m_lineageState->lastDisplaySequence &&
                display->decodedSequence <= *m_lineageState->lastDisplaySequence) ||
            (m_lineageState->lastDisplayPresentation &&
                display->presentation < *m_lineageState->lastDisplayPresentation))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoFilterNode spatial filter changed display timing or order"));
        m_lineageState->lastDisplaySequence = display->decodedSequence;
        m_lineageState->lastDisplayPresentation = display->presentation;
    }
    failure.value().release(); // publish owns the moved frame and failure scope.
    auto buffer = MediaVideoFrameReadinessBinding::publish(std::move(frame), nodeOptions(context), sourceReferenceResource(), m_lineageState->pendingInputAllocations);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }
    const AVFrame* emittedFrame = FFmpegFrameView::frame(buffer.value());
    auto footprint = emittedFrame
        ? MediaFramePayloadFootprint::logicalBytes(
              *emittedFrame, MediaStreamKind::Video)
        : ::media::Result<std::uint64_t>::failure(
              ::media::ErrorInfo::invalidArgument(
                  "VideoFilterNode wrapped frame is unavailable"));
    if (!footprint) return ::media::Status::failure(footprint.error());
    if (auto status = reservation.shrinkToActual(footprint.value());
        !status) {
        return status;
    }
    if (auto status = reservation.attachTo(*buffer.value()); !status) {
        return status;
    }

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = MediaRational{
        m_lineageState->outputTimeBase.num,
        m_lineageState->outputTimeBase.den};
    timeDescriptor.frameRate = MediaRational{m_lineageState->inputFrameRate.num, m_lineageState->inputFrameRate.den};
    buffer.value()->setTimeDescriptor(timeDescriptor);

    AVFrame* outputFrame = FFmpegFrameView::writableFrame(buffer.value());
    if (outputFrame) {
        buffer.value()->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);
    }

    MediaBufferRef output = buffer.value();
    if (lineage) {
        auto canonical = MediaCanonicalVideoFrameBuffer::create(output, std::move(lineage), std::nullopt);
        if (!canonical) return ::media::Status::failure(canonical.error());
        output = std::move(canonical).value();
    } else if (display) {
        auto source = MediaVideoDecodeBuffer::withDisplayLineage(
            *sourceReferenceResource(), output, std::move(display));
        if (!source) return ::media::Status::failure(source.error());
        output = std::move(source).value();
    }
    m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::Resolved, output);
    auto status = emitOutput(context, "frame", output);
    if (status && !m_firstOutputDiagnosticEmitted) {
        filterLog(MediaGraphDiagnosticLevel::State,
                  "trace stage=first_output " +
                      MediaVideoFrameContractValidator::describe(*outputFrame, outputFacts.value()));
        m_firstOutputDiagnosticEmitted = true;
    }
    return status;
}



::media::Status VideoFilterNode::rescaleAndValidateFrame(AVFrame* frame) noexcept
{
    if (!frame || !rationalKnown(m_lineageState->outputTimeBase)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode filtered frame is invalid"));
    }

    if (frame->pts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFilterNode filtered frame has invalid pts"));
    }

    if (m_executionPlan->frameProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint) {
        if (!rationalKnown(m_lineageState->sinkTimeBase))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoFilterNode display frame requires a valid sink time base"));
        frame->time_base = m_lineageState->sinkTimeBase;
        return ::media::Status::success();
    }
    const int64_t ptsIn = frame->pts;
    auto rescaledPts = rescaleStrictlyIncreasingTimestamp(frame->pts,
                                                          m_lineageState->sinkTimeBase,
                                                          m_lineageState->outputTimeBase,
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
            << " output_tb=" << rationalText(
                   m_lineageState->outputTimeBase)
            << " pts_in=" << ptsIn
            << " pts_out=" << frame->pts
            << " fmt=" << pixelFormatName(static_cast<AVPixelFormat>(frame->format))
            << " duration=" << frame->duration;
        filterLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
