#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

#include "internal/graph/nodes/audio/AudioMonotonicTimestamp.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
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

#if LIBAVUTIL_VERSION_MAJOR >= 57
bool sameChannelLayout(const AVChannelLayout& left, const AVChannelLayout& right) noexcept
{
    return av_channel_layout_compare(&left, &right) == 0;
}
#endif

} // namespace

AudioResampleNode::AudioResampleNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "AudioResampleNode")
{
}

MediaNodeKind AudioResampleNode::staticKind() noexcept
{
    return MediaNodeKind::AudioResample;
}

::media::Status AudioResampleNode::start(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    if (auto status = configureCorrection(context); !status) {
        return status;
    }
    return FFmpegCodecNodeRuntime::start(context);
}
::media::Status AudioResampleNode::flush(MediaGraphExecutionContext& context)
{
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
    m_swr.reset(); m_correctionExecutor.reset(); m_nextOutputPts = AV_NOPTS_VALUE;
    m_outputSampleIndex = 0; m_pendingOutputs.clear(); m_pendingInput.reset();
    m_pendingTerminal.reset(); m_drainingEof = false;
    m_drainingClosedInput = false; m_terminals.reset();
    m_lifecycleFlushRequested = false; m_eofEmitted = false;
    m_preferCorrection = true;
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
    if (!m_pendingOutputs.empty()) {
        return processProgress(emitNextPending(context));
    }
    if (m_pendingTerminal && !m_drainingEof) {
        MediaBufferRef terminal = std::move(m_pendingTerminal);
        if (terminal->isEof()) {
            if (auto status = m_correctionExecutor->settleTerminal(); !status) {
                return ::media::Result<MediaNodeProcessResult>::failure(status.error());
            }
            m_terminals.markEof("frame");
            m_eofEmitted = true;
            return processFinished(emitOutput(context, "frame", terminal));
        }
        return processProgress(emitOutput(context, "frame", terminal));
    }
    if (m_terminals.finished()) {
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
            m_terminals.markClosed("frame");
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
            m_terminals.markEof("frame");
            m_eofEmitted = true;
            return processFinished(emitOutput(context, "frame", terminal));
        }
        return processProgress(emitOutput(context, "frame", terminal));
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
            m_terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const bool eof = frameInput.value()->get()->isEof();
    const bool flush = frameInput.value()->get()->isFlush();
    if (eof && m_eofEmitted) {
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
            m_terminals.markEof("frame");
            m_eofEmitted = true;
            return processFinished(emitOutput(context, "frame", terminal));
        }
        return processProgress(emitOutput(context, "frame", terminal));
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
    if (!FFmpegFrameView::frame(buffer)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode expected frame buffer"));
    }
    m_pendingInput = PendingInput{buffer, false};
    return processPendingInputQuantum(context);
}

bool AudioResampleNode::frameMatchesEncoder(const AVFrame* frame) const noexcept
{
    const AVCodecContext* target = codecContext();
    if (!frame || !target) {
        return false;
    }
    if (frame->format != target->sample_fmt || frame->sample_rate != target->sample_rate) {
        return false;
    }
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return sameChannelLayout(frame->ch_layout, target->ch_layout);
#else
    const int frameChannels = frame->channels > 0 ? frame->channels : av_get_channel_layout_nb_channels(frame->channel_layout);
    const int targetChannels = target->channels > 0 ? target->channels : av_get_channel_layout_nb_channels(target->channel_layout);
    return frameChannels == targetChannels && frame->channel_layout == target->channel_layout;
#endif
}

::media::Status AudioResampleNode::ensureSwrInitialized(const AVFrame* inputFrame)
{
    if (m_swr) {
        return ::media::Status::success();
    }
    if (!inputFrame || !codecContext()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized("AudioResampleNode requires input frame and encoder context"));
    }
    if (inputFrame->sample_rate <= 0 || codecContext()->sample_rate <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode requires known sample rates"));
    }

#if LIBAVUTIL_VERSION_MAJOR >= 57
    if (inputFrame->ch_layout.nb_channels <= 0 || codecContext()->ch_layout.nb_channels <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode requires known channel layouts"));
    }
    SwrContext* raw = nullptr;
    const int allocRet = swr_alloc_set_opts2(&raw,
                                             &codecContext()->ch_layout,
                                             codecContext()->sample_fmt,
                                             codecContext()->sample_rate,
                                             &inputFrame->ch_layout,
                                             static_cast<AVSampleFormat>(inputFrame->format),
                                             inputFrame->sample_rate,
                                             0,
                                             nullptr);
    if (allocRet < 0) {
        return FFmpegGraphError::statusFromCode(allocRet, "swr_alloc_set_opts2(audio)");
    }
    m_swr.reset(raw);
#else
    const int64_t inputLayout = inputFrame->channel_layout ? inputFrame->channel_layout : av_get_default_channel_layout(inputFrame->channels);
    const int64_t outputLayout = codecContext()->channel_layout ? codecContext()->channel_layout : av_get_default_channel_layout(codecContext()->channels);
    if (!inputLayout || !outputLayout) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode requires known channel layout"));
    }
    m_swr.reset(swr_alloc_set_opts(nullptr,
                                   outputLayout,
                                   codecContext()->sample_fmt,
                                   codecContext()->sample_rate,
                                   inputLayout,
                                   static_cast<AVSampleFormat>(inputFrame->format),
                                   inputFrame->sample_rate,
                                   0,
                                   nullptr));
    if (!m_swr) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("swr_alloc_set_opts(audio)"));
    }
#endif

    const int initRet = swr_init(m_swr.get());
    return initRet < 0 ? FFmpegGraphError::statusFromCode(initRet, "swr_init(audio)") : ::media::Status::success();
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
    if (!m_pendingInput->submitted && frameMatchesEncoder(inputFrame) &&
        m_correctionExecutor &&
        m_correctionExecutor->mode() == MediaAudioCorrectionExecutionMode::Disabled) {
        auto cloned = FFmpegBufferFactory::cloneFrame(
            inputFrame, MediaStreamKind::Audio);
        if (!cloned) return ::media::Status::failure(cloned.error());
        m_pendingInput.reset();
        if (auto status = stampAndQueue(cloned.value(), inputFrame->pts, srcTb); !status) {
            return status;
        }
        return emitNextPending(context);
    }
    if (auto status = ensureSwrInitialized(inputFrame); !status) return status;
    const int inputSamples = m_pendingInput->submitted ? 0 : inputFrame->nb_samples;
    m_pendingInput->submitted = true;
    return convertQuantum(
        context,
        const_cast<const uint8_t**>(inputFrame->extended_data),
        inputSamples,
        inputFrame->pts,
        srcTb,
        true);
}

::media::Status AudioResampleNode::processEofDrainQuantum(
    MediaGraphExecutionContext& context)
{
    if (!m_swr) {
        m_drainingEof = false;
        return ::media::Status::success();
    }
    if (m_correctionExecutor->requiresNextWindow()) {
        const int preflight = swr_get_out_samples(m_swr.get(), 0);
        if (preflight < 0) {
            return FFmpegGraphError::statusFromCode(
                preflight, "swr_get_out_samples(audio terminal preflight)");
        }
        if (preflight == 0) {
            m_drainingEof = false;
            return ::media::Status::success();
        }
    }
    const AVRational dstTb {1, codecContext()->sample_rate};
    return convertQuantum(
        context, nullptr, 0, AV_NOPTS_VALUE, dstTb, false);
}

::media::Status AudioResampleNode::stampAndQueue(
    MediaBufferRef outputBuffer,
    std::int64_t inputPts,
    AVRational srcTb)
{
    const AVRational dstTb { 1, codecContext()->sample_rate };
    AVFrame* outputFrame = FFmpegFrameView::writableFrame(outputBuffer);
    if (!outputFrame) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode output frame is invalid"));
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
    m_nextOutputPts = nextPts.value();
    if (outputFrame->nb_samples < 0 ||
        m_outputSampleIndex > std::numeric_limits<std::int64_t>::max() -
                                  outputFrame->nb_samples) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleNode output sample index overflows"));
    }
    m_outputSampleIndex += outputFrame->nb_samples;

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = MediaRational{ dstTb.num, dstTb.den };
    outputBuffer->setTimeDescriptor(timeDescriptor);
    outputBuffer->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);
    m_pendingOutputs.push_back(std::move(outputBuffer));
    return ::media::Status::success();
}

::media::Status AudioResampleNode::convertQuantum(
    MediaGraphExecutionContext& context,
    const uint8_t** inputData,
    int inputSamples,
    std::int64_t inputPts,
    AVRational srcTb,
    bool liveInput)
{
    auto correctionWindow = m_correctionExecutor->prepare(
        m_swr.get(), m_outputSampleIndex);
    if (!correctionWindow) {
        return ::media::Status::failure(correctionWindow.error());
    }
    const int availableSamples = swr_get_out_samples(m_swr.get(), inputSamples);
    if (availableSamples < 0) {
        return FFmpegGraphError::statusFromCode(
            availableSamples, "swr_get_out_samples(audio)");
    }
    const int outSamples = std::min(
        availableSamples, correctionWindow.value().maximumOutputSamples);
    if (outSamples <= 0) {
        if (liveInput) m_pendingInput.reset();
        else m_drainingEof = false;
        return ::media::Status::success();
    }
    auto outputFrame = ::media::ffmpeg::makeFrame();
    if (!outputFrame) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "AudioResampleNode failed to allocate output frame"));
    }
    outputFrame->format = codecContext()->sample_fmt;
    outputFrame->sample_rate = codecContext()->sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const int layoutRet = av_channel_layout_copy(
        &outputFrame->ch_layout, &codecContext()->ch_layout);
    if (layoutRet < 0) {
        return FFmpegGraphError::statusFromCode(
            layoutRet, "av_channel_layout_copy(audio resample)");
    }
#else
    outputFrame->channel_layout = codecContext()->channel_layout;
    outputFrame->channels = codecContext()->channels;
#endif
    outputFrame->nb_samples = outSamples;
    const int bufferRet = av_frame_get_buffer(outputFrame.get(), 0);
    if (bufferRet < 0) {
        return FFmpegGraphError::statusFromCode(
            bufferRet, "av_frame_get_buffer(audio resample)");
    }
    const int convertRet = swr_convert(
        m_swr.get(), outputFrame->data, outSamples,
        inputData, inputSamples);
    if (convertRet < 0) {
        return FFmpegGraphError::statusFromCode(convertRet, "swr_convert(audio)");
    }
    if (liveInput) {
        if (convertRet < outSamples) m_pendingInput.reset();
    } else {
        m_drainingEof = convertRet > 0;
    }
    if (convertRet == 0) return ::media::Status::success();
    outputFrame->nb_samples = convertRet;
    if (auto status = m_correctionExecutor->advance(convertRet); !status) return status;
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(outputFrame), MediaStreamKind::Audio);
    if (!wrapped) return ::media::Status::failure(wrapped.error());
    if (auto status = stampAndQueue(wrapped.value(), inputPts, srcTb); !status) {
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
