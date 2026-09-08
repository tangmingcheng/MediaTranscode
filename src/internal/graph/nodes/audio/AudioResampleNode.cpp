#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

#include "internal/graph/nodes/audio/AudioMonotonicTimestamp.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"
#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

AVRational toAVRational(MediaRational value) noexcept
{
    return AVRational{ value.num, value.den };
}

bool known(AVRational value) noexcept
{
    return value.num > 0 && value.den > 0;
}

AVRational sourceTimeBase(const AVFrame* frame, const MediaBufferRef& buffer) noexcept
{
    if (buffer && buffer->timeDescriptor().timeBase.isKnown()) {
        return toAVRational(buffer->timeDescriptor().timeBase);
    }
    if (frame && frame->sample_rate > 0) {
        return AVRational{ 1, frame->sample_rate };
    }
    return AVRational{ 0, 1 };
}

} // namespace

AudioResampleNode::AudioResampleNode(
    MediaNodeId nodeId,
    MediaAudioLineageExecutionMode lineageMode,
    std::shared_ptr<AudioResampleLineageState> lineageState)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "AudioResampleNode")
    , m_lineageState(std::move(lineageState))
    , m_lineageMapper(m_lineageState)
    , m_swrSession(m_lineageState)
    , m_swr(m_lineageState->swr)
    , m_correctionExecutor(m_lineageState->correctionExecutor)
    , m_nextOutputPts(m_lineageState->nextOutputPts)
    , m_outputSampleIndex(m_lineageState->outputSampleIndex)
    , m_pendingOutputs(m_lineageState->pendingOutputs)
    , m_pendingInput(m_lineageState->pendingInput)
    , m_pendingTerminal(m_lineageState->pendingTerminal)
    , m_drainingEof(m_lineageState->drainingEof)
    , m_drainingClosedInput(m_lineageState->drainingClosedInput)
    , m_lifecycleFlushRequested(m_lineageState->lifecycleFlushRequested)
    , m_preferCorrection(m_lineageState->preferCorrection)
    , m_lineageMode(lineageMode)
    , m_activeOrigin(m_lineageState->activeOrigin)
    , m_outputIntervals(m_lineageState->outputIntervals)
    , m_sampleProjection(m_lineageState->sampleProjection)
    , m_lastOutputLineage(m_lineageState->lastOutputLineage)
{
}

MediaNodeKind AudioResampleNode::staticKind() noexcept
{
    return MediaNodeKind::AudioResample;
}

std::string_view AudioResampleNode::generationPurgeIdentity() noexcept
{
    return MediaAudioResampleLineageIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
AudioResampleNode::generationPurgeTarget() const noexcept
{
    return m_lineageState->synchronized() ? m_lineageState : nullptr;
}

bool AudioResampleNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(buffer.get());
    return m_lineageState && m_lineageState->pendingOutputIsCurrent(
        buffer, bound ? std::optional<std::uint64_t>(
                            bound->audioOrigin().generation)
                      : std::nullopt);
}

::media::Status AudioResampleNode::start(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    resetRuntimeState();
    if (auto status = configureCorrection(context); !status) {
        return status;
    }
    return FFmpegCodecNodeRuntime::start(context);
}
::media::Status AudioResampleNode::flush(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    if (!m_correctionExecutor) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleNode flush requires started node"));
    }
    m_lifecycleFlushRequested = true;
    m_drainingEof = m_swr != nullptr;
    return FFmpegCodecNodeRuntime::flush(context);
}
::media::Status AudioResampleNode::stop(MediaGraphExecutionContext& context) { auto status = FFmpegCodecNodeRuntime::stop(context); resetRuntimeState(); return status; }
void AudioResampleNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegCodecNodeRuntime::abort(context); resetRuntimeState(); }
void AudioResampleNode::resetRuntimeState() noexcept
{
    auto lineageLock = m_lineageState->lock();
    m_lineageState->resetForLifecycle();
}

::media::Status AudioResampleNode::emitTerminal(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& terminal)
{
    if (auto freshness = m_lineageState->authorizeRetainedControl(terminal);
        !freshness) {
        return freshness;
    }
    return emitOutput(context, "frame", terminal);
}

::media::Result<bool> AudioResampleNode::consumeCorrection(
    MediaGraphExecutionContext& context)
{
    if (!m_correctionExecutor ||
        m_correctionExecutor->mode() !=
            MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired ||
        !m_correctionExecutor->canAccept()) {
        return ::media::Result<bool>::success(false);
    }
    auto input = tryPopInputOptional(context, "correction");
    if (!input) {
        return ::media::Result<bool>::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Result<bool>::success(false);
    }
    const auto* correctionBuffer = dynamic_cast<const MediaAudioCorrectionBuffer*>(
        input.value()->get());
    if (!correctionBuffer) {
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleNode correction input requires MediaAudioCorrectionBuffer"));
    }
    auto status = m_correctionExecutor->enqueue(correctionBuffer->command());
    if (!status) {
        return ::media::Result<bool>::failure(status.error());
    }
    return ::media::Result<bool>::success(true);
}

::media::Status AudioResampleNode::configureCorrection(
    MediaGraphExecutionContext& context)
{
    auto mode = requiredNodeOption(
        nodeOptions(context), "AudioResampleNode", MediaAudioCorrectionOptionKey::Mode);
    if (!mode) {
        return ::media::Status::failure(mode.error());
    }
    auto parsedMode = parseMediaAudioCorrectionExecutionMode(mode.value());
    if (!parsedMode) {
        return ::media::Status::failure(parsedMode.error());
    }
    if (parsedMode.value() == MediaAudioCorrectionExecutionMode::Disabled) {
        auto executor = AudioSwrCompensationExecutor::create(
            MediaAudioCorrectionExecutionMode::Disabled, 0, 0);
        if (!executor) {
            return ::media::Status::failure(executor.error());
        }
        m_correctionExecutor = std::move(executor).value();
        return ::media::Status::success();
    }
    if (parsedMode.value() ==
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        auto generation = requiredPositiveInt64NodeOption(
            nodeOptions(context),
            "AudioResampleNode",
            MediaAudioCorrectionOptionKey::Generation);
        if (!generation) {
            return ::media::Status::failure(generation.error());
        }
        auto lookahead = requiredPositiveInt64NodeOption(
            nodeOptions(context), "AudioResampleNode",
            MediaAudioCorrectionOptionKey::LookaheadWindows);
        if (!lookahead || lookahead.value() >
                MediaAudioDriftServoLimits::MaximumCorrectionLookaheadWindows) {
            return ::media::Status::failure(
                lookahead ? ::media::ErrorInfo::invalidArgument(
                                "AudioResampleNode correction lookahead exceeds limit")
                          : lookahead.error());
        }
        auto executor = AudioSwrCompensationExecutor::create(
            MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired,
            static_cast<std::uint64_t>(generation.value()),
            static_cast<std::size_t>(lookahead.value()));
        if (!executor) {
            return ::media::Status::failure(executor.error());
        }
        m_correctionExecutor = std::move(executor).value();
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "AudioResampleNode audio correction mode is invalid"));
}

::media::Result<MediaNodeProcessResult> AudioResampleNode::onProcess(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    if (!m_pendingOutputs.empty()) {
        return processProgress(emitNextPending(context));
    }
    if (m_pendingTerminal && !m_drainingEof) {
        MediaBufferRef terminal = std::move(m_pendingTerminal);
        if (terminal->isEof()) {
            if (auto status = m_correctionExecutor->settleTerminal(); !status) {
                return ::media::Result<MediaNodeProcessResult>::failure(status.error());
            }
            m_lineageState->terminals.markEof("frame");
            m_lineageState->eofEmitted = true;
            return processFinished(emitTerminal(context, terminal));
        }
        return processProgress(emitTerminal(context, terminal));
    }
    if (m_lineageState->terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }
    if (m_lifecycleFlushRequested && !m_drainingEof) {
        m_lifecycleFlushRequested = false;
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::progress());
    }

    auto bindStatus = bindEncoderContext(context);
    if (!bindStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(bindStatus.error());
    }
    if (!hasCodecContext()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    if (m_preferCorrection) {
        auto consumedCorrection = consumeCorrection(context);
        if (!consumedCorrection) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                consumedCorrection.error());
        }
        if (consumedCorrection.value()) {
            m_preferCorrection = false;
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::progress());
        }
    }

    if (m_pendingInput) {
        m_preferCorrection = true;
        auto status = processPendingInputQuantum(context);
        return processProgress(std::move(status));
    }
    if (m_drainingEof) {
        m_preferCorrection = true;
        auto status = processEofDrainQuantum(context);
        if (!status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        if (m_drainingEof) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::progress());
        }
        if (m_drainingClosedInput) {
            m_drainingClosedInput = false;
            if (auto settle = m_correctionExecutor->settleTerminal(); !settle) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    settle.error());
            }
            m_lineageState->terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::finished());
        }
        if (m_lifecycleFlushRequested) {
            m_lifecycleFlushRequested = false;
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::progress());
        }
        MediaBufferRef terminal = std::move(m_pendingTerminal);
        if (terminal->isEof()) {
            if (auto settle = m_correctionExecutor->settleTerminal(); !settle) {
                return ::media::Result<MediaNodeProcessResult>::failure(settle.error());
            }
            m_lineageState->terminals.markEof("frame");
            m_lineageState->eofEmitted = true;
            return processFinished(emitTerminal(context, terminal));
        }
        return processProgress(emitTerminal(context, terminal));
    }

    auto frameInput = tryPopInputOptional(context, "frame");
    if (!frameInput) {
        return ::media::Result<MediaNodeProcessResult>::failure(frameInput.error());
    }
    if (!frameInput.value()) {
        auto consumedCorrection = consumeCorrection(context);
        if (!consumedCorrection) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                consumedCorrection.error());
        }
        if (consumedCorrection.value()) {
            m_preferCorrection = false;
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::progress());
        }
        MediaChannel* frameChannel = context.findInputChannel(nodeId(), "frame");
        if (frameChannel && frameChannel->closed()) {
            if (m_swr) {
                m_drainingClosedInput = true;
                m_drainingEof = true;
                auto status = processEofDrainQuantum(context);
                if (!status) {
                    return ::media::Result<MediaNodeProcessResult>::failure(
                        status.error());
                }
                if (m_drainingEof) {
                    return ::media::Result<MediaNodeProcessResult>::success(
                        MediaNodeProcessResult::progress());
                }
                m_drainingClosedInput = false;
            }
            if (auto settle = m_correctionExecutor->settleTerminal(); !settle) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    settle.error());
            }
            m_lineageState->terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const bool eof = frameInput.value()->get()->isEof();
    const bool flush = frameInput.value()->get()->isFlush();
    if (eof && m_lineageState->eofEmitted) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }
    if (eof || flush) {
        m_pendingTerminal = *frameInput.value();
        m_drainingEof = m_swr != nullptr;
        auto drainStatus = processEofDrainQuantum(context);
        if (!drainStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(drainStatus.error());
        }
        if (m_drainingEof) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::progress());
        }
        MediaBufferRef terminal = std::move(m_pendingTerminal);
        if (eof) {
            if (auto settle = m_correctionExecutor->settleTerminal(); !settle) {
                return ::media::Result<MediaNodeProcessResult>::failure(settle.error());
            }
            m_lineageState->terminals.markEof("frame");
            m_lineageState->eofEmitted = true;
            return processFinished(emitTerminal(context, terminal));
        }
        return processProgress(emitTerminal(context, terminal));
    }
    m_preferCorrection = true;
    auto processStatus = processFrame(context, *frameInput.value());
    if (!processStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(processStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::progress());
}

::media::Status AudioResampleNode::bindEncoderContext(MediaGraphExecutionContext& context)
{
    if (hasCodecContext()) {
        return ::media::Status::success();
    }

    auto codecInput = tryPopInputOptional(context, "codec");
    if (!codecInput) {
        return ::media::Status::failure(codecInput.error());
    }
    if (!codecInput.value()) {
        MediaChannel* codecChannel = context.findInputChannel(nodeId(), "codec");
        if (codecChannel && codecChannel->closed()) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "AudioResampleNode codec metadata closed before binding"));
        }
        return ::media::Status::success();
    }

    if (!tryBindCodecContext(*codecInput.value())) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode expected encoder codec context"));
    }
    return ::media::Status::success();
}

::media::Status AudioResampleNode::processFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    if (buffer->isEof() || buffer->isFlush()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleNode terminal buffer bypassed boundary state"));
    }
    MediaBufferRef media = buffer;
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(buffer.get());
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        if (!bound || !codecContext() ||
            bound->audioOrigin().outputSampleRate != codecContext()->sample_rate) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Synchronized AudioResampleNode requires bound audio matching planned output rate"));
        }
        const AVFrame* synchronizedFrame = FFmpegFrameView::frame(
            bound->media()->media());
        if (!synchronizedFrame) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Synchronized AudioResampleNode requires an exact audio frame"));
        }
        if (auto status = m_lineageMapper.acceptInput(
                *bound, *synchronizedFrame, codecContext()->sample_rate);
            !status) {
            return status;
        }
        media = bound->media()->media();
    } else if (bound) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Legacy AudioResampleNode rejects bound canonical audio"));
    }
    if (!FFmpegFrameView::frame(media)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode expected frame buffer"));
    }
    m_pendingInput = AudioResamplePendingInput{media, false};
    return processPendingInputQuantum(context);
}

::media::Status AudioResampleNode::processPendingInputQuantum(
    MediaGraphExecutionContext& context)
{
    if (!m_pendingInput || !m_pendingInput->buffer) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleNode has no pending live input"));
    }
    const AVFrame* inputFrame = FFmpegFrameView::frame(m_pendingInput->buffer);
    if (!inputFrame) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleNode pending input is not a frame"));
    }
    const AVRational srcTb = sourceTimeBase(inputFrame, m_pendingInput->buffer);
    if (!known(srcTb)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleNode requires known frame time_base"));
    }
    if (!m_pendingInput->submitted && codecContext() &&
        m_swrSession.frameMatchesTarget(*inputFrame, *codecContext()) &&
        m_correctionExecutor &&
        m_correctionExecutor->mode() == MediaAudioCorrectionExecutionMode::Disabled) {
        auto cloned = FFmpegBufferFactory::cloneFrame(
            m_pendingInput->buffer, MediaStreamKind::Audio);
        if (!cloned) return ::media::Status::failure(cloned.error());
        const auto& inheritedCredit =
            FFmpegFrameView::payloadCredit(m_pendingInput->buffer);
        if (!inheritedCredit && context.payloadCreditsRequired()) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "AudioResampleNode alias input lacks payload credit"));
        }
        std::optional<MediaGraphPayloadReservation> nonRealtimeReservation;
        if (!inheritedCredit) {
            auto reservation = context.reservePayload(
                nodeId(), MediaStreamKind::Audio, MediaPayloadKind::Frame);
            if (!reservation) return ::media::Status::failure(reservation.error());
            nonRealtimeReservation.emplace(std::move(reservation).value());
        }
        m_pendingInput.reset();
        if (auto status = stampAndQueue(
                cloned.value(), inputFrame->pts, srcTb,
                std::move(nonRealtimeReservation)); !status) {
            return status;
        }
        return emitNextPending(context);
    }
    if (!codecContext()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleNode requires encoder context"));
    }
    if (auto status = m_swrSession.ensureInitialized(
            *inputFrame, *codecContext()); !status) {
        return status;
    }
    const int inputSamples = m_pendingInput->submitted ? 0 : inputFrame->nb_samples;
    return convertLiveQuantum(
        context,
        const_cast<const uint8_t**>(inputFrame->extended_data),
        inputSamples,
        inputFrame->pts,
        srcTb);
}

::media::Status AudioResampleNode::processEofDrainQuantum(
    MediaGraphExecutionContext& context)
{
    if (!m_swr) {
        m_drainingEof = false;
        return finishBypassLineage();
    }
    if (m_correctionExecutor->requiresNextWindow()) {
        if (!m_sampleProjection) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "AudioResampleNode terminal delay requires sample projection"));
        }
        auto evidence = m_swrSession.inspectDrainEvidence(
            m_sampleProjection->sourceSampleRate(),
            codecContext()->sample_rate);
        if (!evidence) return ::media::Status::failure(evidence.error());
        if (evidence.value() == AudioSwrDrainEvidence::NoDelay) {
            return drainSwrQuantum(context, false);
        }
    }
    return drainSwrQuantum(context, true);
}

::media::Status AudioResampleNode::finishBypassLineage()
{
    if (m_lineageMode !=
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        return ::media::Status::success();
    }
    const auto authorized = m_correctionExecutor
        ? m_correctionExecutor->outstandingAuthorizedDroppedSamples()
        : 0;
    if (authorized != 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleNode bypass cannot finish with correction residue"));
    }
    return m_lineageMapper.settleDroppedSamples(0);
}

::media::Status AudioResampleNode::settleLineageResidue(
    AudioSwrResamplerExhausted exhaustionProof)
{
    if (m_lineageMode !=
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        return ::media::Status::success();
    }
    return m_lineageMapper.settleExhaustedResidue(exhaustionProof);
}

::media::Status AudioResampleNode::stampAndQueue(
    MediaBufferRef outputBuffer,
    std::int64_t inputPts,
    AVRational srcTb,
    std::optional<MediaGraphPayloadReservation> reservation)
{
    const AVRational dstTb { 1, codecContext()->sample_rate };
    AVFrame* outputFrame = FFmpegFrameView::writableFrame(outputBuffer);
    if (!outputFrame) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode output frame is invalid"));
    }
    auto footprint = MediaFramePayloadFootprint::logicalBytes(
        *outputFrame, MediaStreamKind::Audio);
    if (!footprint) return ::media::Status::failure(footprint.error());
    if (reservation) {
        if (auto status = reservation->shrinkToActual(footprint.value()); !status)
            return status;
        if (auto status = reservation->attachTo(*outputBuffer); !status)
            return status;
    } else if (!FFmpegFrameView::payloadCredit(outputBuffer)) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleNode alias output lacks inherited payload credit"));
    }
    auto pts = m_nextOutputPts != AV_NOPTS_VALUE
        ? ::media::Result<int64_t>::success(m_nextOutputPts)
        : monotonicAudioFrameTimestamp(inputPts, srcTb, dstTb, m_nextOutputPts);
    if (!pts) {
        return ::media::Status::failure(pts.error());
    }
    outputFrame->pts = pts.value();
    outputFrame->pkt_dts = AV_NOPTS_VALUE;
    outputFrame->duration = outputFrame->nb_samples;

    auto nextPts = nextAudioFrameTimestamp(outputFrame->pts, outputFrame->nb_samples);
    if (!nextPts) {
        return ::media::Status::failure(nextPts.error());
    }
    if (outputFrame->nb_samples <= 0 ||
        m_outputSampleIndex > std::numeric_limits<std::int64_t>::max() -
                                  outputFrame->nb_samples) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleNode output sample index overflows"));
    }
    const std::int64_t nextOutputSampleIndex =
        m_outputSampleIndex + outputFrame->nb_samples;

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = MediaRational{ dstTb.num, dstTb.den };
    outputBuffer->setTimeDescriptor(timeDescriptor);
    outputBuffer->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        auto bound = m_lineageMapper.bindOutput(
            outputBuffer, outputFrame->nb_samples);
        if (!bound) return ::media::Status::failure(bound.error());
        outputBuffer = std::move(bound).value();
    }
    m_nextOutputPts = nextPts.value();
    m_outputSampleIndex = nextOutputSampleIndex;
    m_pendingOutputs.push_back(std::move(outputBuffer));
    return ::media::Status::success();
}

::media::Status AudioResampleNode::convertLiveQuantum(
    MediaGraphExecutionContext& context,
    const uint8_t** inputData,
    int inputSamples,
    std::int64_t inputPts,
    AVRational srcTb)
{
    auto reservation = context.reservePayload(
        nodeId(), MediaStreamKind::Audio, MediaPayloadKind::Frame);
    if (!reservation) return ::media::Status::failure(reservation.error());
    auto correctionWindow = m_correctionExecutor->prepare(
        m_swr.get(), m_outputSampleIndex);
    if (!correctionWindow) {
        return ::media::Status::failure(correctionWindow.error());
    }
    if (!codecContext()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleNode requires encoder context"));
    }
    auto conversion = m_swrSession.convertLive(
        inputData, inputSamples,
        correctionWindow.value().maximumOutputSamples,
        *codecContext());
    if (!conversion) {
        return ::media::Status::failure(conversion.error());
    }
    auto converted = std::move(conversion).value();
    if (converted.capacity <= 0) {
        return ::media::Status::success();
    }
    if (m_pendingInput) {
        m_pendingInput->submitted = true;
    }
    if (converted.produced < converted.capacity) m_pendingInput.reset();
    if (converted.produced == 0) {
        return ::media::Status::success();
    }
    if (auto status = m_correctionExecutor->advance(converted.produced);
        !status) {
        return status;
    }
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(converted.output), MediaStreamKind::Audio);
    if (!wrapped) return ::media::Status::failure(wrapped.error());
    if (auto status = stampAndQueue(
            wrapped.value(), inputPts, srcTb,
            std::optional<MediaGraphPayloadReservation>(
                std::move(reservation).value())); !status) {
        return status;
    }
    return emitNextPending(context);
}

::media::Status AudioResampleNode::drainSwrQuantum(
    MediaGraphExecutionContext& context,
    bool correctionWindowRequired)
{
    auto reservation = context.reservePayload(
        nodeId(), MediaStreamKind::Audio, MediaPayloadKind::Frame);
    if (!reservation) return ::media::Status::failure(reservation.error());
    if (!codecContext()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleNode drain requires encoder context"));
    }
    int maximumOutputSamples = 1;
    if (correctionWindowRequired) {
        auto correctionWindow = m_correctionExecutor->prepare(
            m_swr.get(), m_outputSampleIndex);
        if (!correctionWindow) {
            return ::media::Status::failure(correctionWindow.error());
        }
        maximumOutputSamples = correctionWindow.value().maximumOutputSamples;
    }
    auto conversion = m_swrSession.drainQuantum(
        maximumOutputSamples, *codecContext());
    if (!conversion) {
        return ::media::Status::failure(conversion.error());
    }
    auto converted = std::move(conversion).value();
    m_drainingEof = converted.produced > 0;
    if (converted.produced == 0) {
        if (!converted.exhausted) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "AudioResampleNode zero drain lacks exhaustion proof"));
        }
        const bool finalDrain = m_drainingClosedInput ||
            (m_pendingTerminal && m_pendingTerminal->isEof());
        if (!finalDrain) return ::media::Status::success();
        if (auto status = m_correctionExecutor->settleTerminal(
                *converted.exhausted); !status) {
            return status;
        }
        return settleLineageResidue(*converted.exhausted);
    }
    if (!correctionWindowRequired) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "AudioResampleNode no-delay evidence produced unplanned output"));
    }
    if (auto status = m_correctionExecutor->advance(converted.produced);
        !status) {
        return status;
    }
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(converted.output), MediaStreamKind::Audio);
    if (!wrapped) return ::media::Status::failure(wrapped.error());
    const AVRational dstTb {1, codecContext()->sample_rate};
    if (auto status = stampAndQueue(
            wrapped.value(), AV_NOPTS_VALUE, dstTb,
            std::optional<MediaGraphPayloadReservation>(
                std::move(reservation).value())); !status) {
        return status;
    }
    return emitNextPending(context);
}

::media::Status AudioResampleNode::emitNextPending(
    MediaGraphExecutionContext& context)
{
    if (m_pendingOutputs.empty()) {
        return ::media::Status::success();
    }
    MediaBufferRef output = std::move(m_pendingOutputs.front());
    m_pendingOutputs.pop_front();
    return emitOutput(context, "frame", output);
}

} // namespace media::ffmpeg::graph
