#include "internal/graph/nodes/audio/AudioResampleNode.h"

#include "internal/graph/nodes/audio/AudioMonotonicTimestamp.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

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

::media::Status AudioResampleNode::onProcess(MediaGraphExecutionContext& context)
{
    auto bindStatus = bindEncoderContext(context);
    if (!bindStatus) {
        return bindStatus;
    }
    if (!hasCodecContext()) {
        return ::media::Status::success();
    }

    auto frameInput = tryPopInputOptional(context, "frame");
    if (!frameInput) {
        return ::media::Status::failure(frameInput.error());
    }
    if (!frameInput.value()) {
        return ::media::Status::success();
    }

    return processFrame(context, *frameInput.value());
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
        return emitOutput(context, "frame", buffer);
    }
    const AVFrame* frame = FFmpegFrameView::frame(buffer);
    if (!frame) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode expected frame buffer"));
    }
    return emitConvertedFrame(context, frame, buffer);
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

::media::Status AudioResampleNode::emitConvertedFrame(MediaGraphExecutionContext& context,
                                                      const AVFrame* inputFrame,
                                                      const MediaBufferRef& inputBuffer)
{
    const AVRational srcTb = sourceTimeBase(inputFrame, inputBuffer);
    const AVRational dstTb { 1, codecContext()->sample_rate };
    if (!known(srcTb) || !known(dstTb)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode requires known frame time_base"));
    }

    MediaBufferRef outputBuffer;
    if (frameMatchesEncoder(inputFrame)) {
        auto cloned = FFmpegBufferFactory::cloneFrame(inputFrame, MediaStreamKind::Audio);
        if (!cloned) {
            return ::media::Status::failure(cloned.error());
        }
        outputBuffer = cloned.value();
    } else {
        auto initStatus = ensureSwrInitialized(inputFrame);
        if (!initStatus) {
            return initStatus;
        }

        auto outputFrame = ::media::ffmpeg::makeFrame();
        if (!outputFrame) {
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed("AudioResampleNode failed to allocate output frame"));
        }
        outputFrame->format = codecContext()->sample_fmt;
        outputFrame->sample_rate = codecContext()->sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
        const int layoutRet = av_channel_layout_copy(&outputFrame->ch_layout, &codecContext()->ch_layout);
        if (layoutRet < 0) {
            return FFmpegGraphError::statusFromCode(layoutRet, "av_channel_layout_copy(audio resample)" );
        }
#else
        outputFrame->channel_layout = codecContext()->channel_layout;
        outputFrame->channels = codecContext()->channels;
#endif
        const int outSamples = swr_get_out_samples(m_swr.get(), inputFrame->nb_samples);
        if (outSamples <= 0) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode calculated non-positive output samples"));
        }
        outputFrame->nb_samples = outSamples;
        const int bufferRet = av_frame_get_buffer(outputFrame.get(), 0);
        if (bufferRet < 0) {
            return FFmpegGraphError::statusFromCode(bufferRet, "av_frame_get_buffer(audio resample)");
        }
        const int convertRet = swr_convert(m_swr.get(),
                                           outputFrame->data,
                                           outSamples,
                                           const_cast<const uint8_t**>(inputFrame->extended_data),
                                           inputFrame->nb_samples);
        if (convertRet < 0) {
            return FFmpegGraphError::statusFromCode(convertRet, "swr_convert(audio)");
        }
        outputFrame->nb_samples = convertRet;
        auto wrapped = FFmpegBufferFactory::wrapFrame(std::move(outputFrame), MediaStreamKind::Audio);
        if (!wrapped) {
            return ::media::Status::failure(wrapped.error());
        }
        outputBuffer = wrapped.value();
    }

    AVFrame* outputFrame = FFmpegFrameView::writableFrame(outputBuffer);
    if (!outputFrame) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("AudioResampleNode output frame is invalid"));
    }
    auto pts = monotonicAudioFrameTimestamp(inputFrame->pts, srcTb, dstTb, m_nextOutputPts);
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

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = MediaRational{ dstTb.num, dstTb.den };
    outputBuffer->setTimeDescriptor(timeDescriptor);
    outputBuffer->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);
    return emitOutput(context, "frame", outputBuffer);
}

} // namespace media::ffmpeg::graph
