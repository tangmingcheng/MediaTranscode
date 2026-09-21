#include "internal/graph/nodes/metadata/CodecResolverNode.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"
#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <sstream>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string missingValue = {})
{
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
}

std::string pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? std::string(name) : std::string("unknown");
}

void codecResolverLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("codec_resolver.") + message);
}

} // namespace

CodecResolverNode::CodecResolverNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "CodecResolverNode")
{
}

MediaNodeKind CodecResolverNode::staticKind() noexcept
{
    return MediaNodeKind::CodecResolver;
}

::media::Status CodecResolverNode::bindPreparedHardwareDevice(AVBufferRef* device)
{
    if (m_emitted || m_preparedDecoder || m_preparedHardwareDevice || !device || !device->data)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source decoder requires one prepared hardware device before runtime start"));
    auto retained = ::media::ffmpeg::BufferRefPtr(av_buffer_ref(device));
    if (!retained) return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
        "Could not retain the prepared source hardware device"));
    m_preparedHardwareDevice = std::move(retained);
    return ::media::Status::success();
}

::media::Status CodecResolverNode::bindPreparedEncoder(MediaBufferRef encoder)
{
    auto* codec = dynamic_cast<FFmpegCodecContextBuffer*>(encoder.get());
    if (m_emitted || m_preparedEncoder || !codec || !codec->context() ||
        !avcodec_is_open(codec->context())) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Output branch requires one opened encoder before runtime start"));
    }
    auto snapshot = FFmpegCodecParametersMaterializer::snapshot(*codec->context());
    if (!snapshot) return ::media::Status::failure(snapshot.error());
    auto readback = MediaVideoEncoderReadback::capture(*codec->context());
    if (!readback) return ::media::Status::failure(readback.error());
    {
        std::lock_guard lock(m_snapshotMutex);
        m_encoderReadback = std::move(readback).value();
        m_encoderParametersSnapshot = std::move(snapshot).value();
    }
    m_preparedEncoder = std::move(encoder);
    return ::media::Status::success();
}

::media::Result<MediaVideoEncoderReadback> CodecResolverNode::encoderReadback() const
{
    std::lock_guard lock(m_snapshotMutex);
    if (!m_encoderReadback) return ::media::Result<MediaVideoEncoderReadback>::failure(
        ::media::ErrorInfo::notInitialized("encoder has not published immutable opened readback"));
    return ::media::Result<MediaVideoEncoderReadback>::success(*m_encoderReadback);
}

MediaBufferRef CodecResolverNode::encoderParametersSnapshot() const
{
    std::lock_guard lock(m_snapshotMutex);
    return m_encoderParametersSnapshot;
}

MediaBufferRef CodecResolverNode::inputSnapshot() const
{
    std::lock_guard lock(m_snapshotMutex);
    return m_inputSnapshot;
}

MediaBufferRef CodecResolverNode::timestampSource() const
{
    std::lock_guard lock(m_snapshotMutex);
    return m_timestampSource;
}

::media::Result<MediaDecoderRuntimeFacts> CodecResolverNode::decoderRuntimeFacts() const
{
    std::lock_guard lock(m_snapshotMutex);
    if (!m_decoderRuntimeFacts) return ::media::Result<MediaDecoderRuntimeFacts>::failure(
        ::media::ErrorInfo::notInitialized("Shared decoder has not published opened codec facts"));
    return ::media::Result<MediaDecoderRuntimeFacts>::success(*m_decoderRuntimeFacts);
}

::media::Result<MediaNodeProcessResult> CodecResolverNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return processFinished();
    }

    const auto mode = nodeOption(context, "codec_resolver.mode");
    if (!mode.empty() && mode != "source_decode" && mode != "output_branch") {
        return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Unknown codec resolver assembly mode")));
    }

    if (mode == "output_branch") {
        if (!m_preparedEncoder) {
            return processProgress(::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "Output branch encoder was not prepared before publication")));
        }
        auto status = emitOutput(context, "encoder", m_preparedEncoder);
        if (!status) return processProgress(std::move(status));
        m_preparedEncoder.reset();
        m_emitted = true;
        return processFinished();
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        return processWaiting();
    }

    auto* formatBuffer = dynamic_cast<FFmpegInputSnapshotBuffer*>(input.value()->get());
    if (!formatBuffer || !formatBuffer->inputSnapshotComplete()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverNode expected complete input snapshots"));
    }

    const FFmpegInputStreamSnapshot* stream = nullptr;
    for (int index = 0; (stream = formatBuffer->inputStreamSnapshot(index)) != nullptr; ++index) {
        if (stream->streamKind == MediaStreamKind::Video) break;
    }
    if (!stream || stream->streamKind != MediaStreamKind::Video) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverNode requires video input snapshot"));
    }

    {
        std::lock_guard lock(m_snapshotMutex);
        m_inputSnapshot = *input.value();
    }

    auto decoderStatus = prepareDecoder(context, *stream);
    if (!decoderStatus) {
        return processProgress(decoderStatus);
    }

    if (mode == "source_decode") {
        if (auto timestamp = timestampSource()) {
            auto status = emitOutput(context, "timestamp_source", std::move(timestamp));
            if (!status) return processProgress(status);
        }
        auto status = emitOutput(context, "decoder", m_preparedDecoder);
        if (!status) return processProgress(status);
        m_preparedDecoder.reset();
        m_emitted = true;
        return processFinished();
    }

    auto encoderStatus = prepareEncoder(context, *stream);
    if (!encoderStatus) {
        return processProgress(encoderStatus);
    }

    // Prepare both codecs before publishing either. The decoder port releases
    // live frames, so publish its configuration only after all target timing
    // metadata is available to downstream workers.
    auto encoderPublished = emitOutput(context, "encoder", m_preparedEncoder);
    if (!encoderPublished) return processProgress(encoderPublished);
    m_preparedEncoder.reset();
    if (auto timestamp = timestampSource()) {
        auto timingPublished = emitOutput(context, "timestamp_source", std::move(timestamp));
        if (!timingPublished) return processProgress(timingPublished);
    }
    auto decoderPublished = emitOutput(context, "decoder", m_preparedDecoder);
    if (!decoderPublished) return processProgress(decoderPublished);
    m_preparedDecoder.reset();
    m_emitted = true;
    return processFinished();
}

::media::Status CodecResolverNode::prepareDecoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream)
{
    auto codecParameters = stream.cloneCodecParameters();
    if (!codecParameters) return ::media::Status::failure(codecParameters.error());
    CodecResolverDecoderContextBuildRequest request{
        codecParameters.value().get(), stream.time, nodeOptions(context), m_preparedHardwareDevice.get()};
    auto built = CodecResolverDecoderContextBuilder::build(request);
    if (!built) return ::media::Status::failure(built.error());
    auto decoder = std::move(built).value();
    m_decoderHardwareDevice = std::move(decoder.hardwareDevice);
    {
        std::lock_guard lock(m_snapshotMutex);
        m_decoderRuntimeFacts = std::move(decoder.runtimeFacts);
    }
    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(decoder.context));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    m_preparedDecoder = std::move(buffer).value();

    if (context.findOutputChannel(nodeId(), "timestamp_source")) {
        auto timestampContext = ::media::ffmpeg::makeCodecContext(nullptr);
        if (!timestampContext) {
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
                "Could not allocate immutable decoder timing metadata"));
        }
        timestampContext->pkt_timebase =
            AVRational{stream.time.timeBase.num, stream.time.timeBase.den};
        timestampContext->time_base = timestampContext->pkt_timebase;
        timestampContext->codec_type = AVMEDIA_TYPE_VIDEO;
        auto timestampBuffer = FFmpegBufferFactory::wrapCodecContext(
            std::move(timestampContext));
        if (!timestampBuffer) return ::media::Status::failure(timestampBuffer.error());
        {
            std::lock_guard lock(m_snapshotMutex);
            m_timestampSource = timestampBuffer.value();
        }
    }

    return ::media::Status::success();
}

::media::Status CodecResolverNode::prepareEncoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream)
{
    const MediaNodeOptions* options = nodeOptions(context);

    auto codecParameters = stream.cloneCodecParameters();
    if (!codecParameters) return ::media::Status::failure(codecParameters.error());

    CodecResolverEncoderContextBuildRequest request;
    request.codecParameters = codecParameters.value().get();
    request.sourceFormat = stream.format;
    request.sourceTime = stream.time;
    request.options = options;
    request.hardwareDevice = m_decoderHardwareDevice.get();

    auto encoderBuildResult = CodecResolverEncoderContextBuilder::build(request);
    if (!encoderBuildResult) {
        return ::media::Status::failure(encoderBuildResult.error());
    }

    CodecResolverEncoderContextBuildResult encoderBuild = std::move(encoderBuildResult).value();
    AVCodecContext* encoderContext = encoderBuild.context.get();
    const AVCodec* encoder = encoderContext ? encoderContext->codec : nullptr;

    codecResolverLog(MediaGraphDiagnosticLevel::State,
                     std::string("encoder.open name=") +
                         (encoder && encoder->name ? encoder->name : optionValue(options, "encoder", "unknown")) +
                         " pix_fmt=" + pixelFormatName(encoderContext ? encoderContext->pix_fmt : AV_PIX_FMT_NONE) +
                         " hw_frames_format=" + pixelFormatName(encoderBuild.hardwareFramesFormat) +
                         " surface_sw_format=" + pixelFormatName(encoderBuild.surfaceSoftwareFormat) +
                         " frame_kind=" + optionValue(options, "encoder.pipeline.frame_kind", "missing") +
                         " hwaccel=" + optionValue(options, "encoder.pipeline.hwaccel", "missing") +
                         " hw_device_ctx=" + (encoderContext && encoderContext->hw_device_ctx ? "set" : "none") +
                         " hw_frames_ctx=" + (encoderContext && encoderContext->hw_frames_ctx ? "set" : "none"));

    auto snapshot = FFmpegCodecParametersMaterializer::snapshot(*encoderContext);
    if (!snapshot) return ::media::Status::failure(snapshot.error());
    auto readback = MediaVideoEncoderReadback::capture(*encoderContext);
    if (!readback) return ::media::Status::failure(readback.error());
    {
        std::lock_guard lock(m_snapshotMutex);
        m_encoderReadback = std::move(readback).value();
        m_encoderParametersSnapshot = std::move(snapshot).value();
    }
    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(encoderBuild.context));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    m_preparedEncoder = std::move(buffer).value();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
