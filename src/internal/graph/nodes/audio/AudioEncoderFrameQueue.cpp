#include "internal/graph/nodes/audio/AudioEncoderFrameQueue.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
}

namespace media::ffmpeg::graph {

AudioEncoderFrameQueue::~AudioEncoderFrameQueue()
{
    reset();
}

::media::Status AudioEncoderFrameQueue::configure(const AVCodecContext& codecContext)
{
    reset();
    if (codecContext.sample_fmt == AV_SAMPLE_FMT_NONE ||
        codecContext.sample_rate <= 0 ||
        codecContext.frame_size <= 0 ||
        codecContext.ch_layout.nb_channels <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioEncoderFrameQueue requires explicit sample format, sample rate, channel layout, and frame size"));
    }

    const int layoutStatus = av_channel_layout_copy(&m_channelLayout, &codecContext.ch_layout);
    if (layoutStatus < 0) {
        return FFmpegGraphError::statusFromCode(layoutStatus, "av_channel_layout_copy(audio encoder frame queue)");
    }

    m_sampleFormat = codecContext.sample_fmt;
    m_sampleRate = codecContext.sample_rate;
    m_frameSize = codecContext.frame_size;
    m_fifo = ::media::ffmpeg::makeAudioFifo(
        m_sampleFormat, m_channelLayout.nb_channels, m_frameSize);
    if (!m_fifo) {
        reset();
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("AudioEncoderFrameQueue failed to allocate AVAudioFifo"));
    }
    return ::media::Status::success();
}

::media::Status AudioEncoderFrameQueue::push(const AVFrame& frame)
{
    if (!configured()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("AudioEncoderFrameQueue is not configured"));
    }
    if (frame.format != m_sampleFormat ||
        frame.sample_rate != m_sampleRate ||
        frame.nb_samples <= 0 ||
        frame.pts == AV_NOPTS_VALUE ||
        av_channel_layout_compare(&frame.ch_layout, &m_channelLayout) != 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioEncoderFrameQueue input does not match the planned encoder format"));
    }

    const int queued = queuedSamples();
    if (m_nextPts == AV_NOPTS_VALUE) {
        m_nextPts = frame.pts;
    } else if (frame.pts != m_nextPts + queued) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioEncoderFrameQueue requires contiguous input timestamps"));
    }

    const int required = queued + frame.nb_samples;
    const int reallocStatus = av_audio_fifo_realloc(m_fifo.get(), required);
    if (reallocStatus < 0) {
        return FFmpegGraphError::statusFromCode(reallocStatus, "av_audio_fifo_realloc(audio encoder frame queue)");
    }

    const int written = av_audio_fifo_write(
        m_fifo.get(), reinterpret_cast<void**>(frame.extended_data), frame.nb_samples);
    if (written != frame.nb_samples) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ffmpegFailure("AudioEncoderFrameQueue failed to enqueue all audio samples"));
    }
    return ::media::Status::success();
}

bool AudioEncoderFrameQueue::hasFullFrame() const noexcept
{
    return configured() && queuedSamples() >= m_frameSize;
}

int AudioEncoderFrameQueue::queuedSamples() const noexcept
{
    return m_fifo ? av_audio_fifo_size(m_fifo.get()) : 0;
}

::media::Result<::media::ffmpeg::FramePtr> AudioEncoderFrameQueue::popFullFrame()
{
    if (!hasFullFrame()) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            ::media::ErrorInfo::wouldBlock("AudioEncoderFrameQueue has no complete encoder frame"));
    }
    return popSamples(m_frameSize);
}

::media::Result<::media::ffmpeg::FramePtr> AudioEncoderFrameQueue::popRemainingFrame()
{
    if (queuedSamples() <= 0) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            ::media::ErrorInfo::wouldBlock("AudioEncoderFrameQueue has no remaining samples"));
    }
    return popSamples(queuedSamples());
}

void AudioEncoderFrameQueue::reset() noexcept
{
    m_fifo.reset();
    av_channel_layout_uninit(&m_channelLayout);
    m_sampleFormat = AV_SAMPLE_FMT_NONE;
    m_sampleRate = 0;
    m_frameSize = 0;
    m_nextPts = AV_NOPTS_VALUE;
}

::media::Result<::media::ffmpeg::FramePtr> AudioEncoderFrameQueue::popSamples(int samples)
{
    auto frame = ::media::ffmpeg::makeFrame();
    if (!frame) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            ::media::ErrorInfo::allocationFailed("AudioEncoderFrameQueue failed to allocate output frame"));
    }
    frame->format = m_sampleFormat;
    frame->sample_rate = m_sampleRate;
    frame->nb_samples = samples;
    const int layoutStatus = av_channel_layout_copy(&frame->ch_layout, &m_channelLayout);
    if (layoutStatus < 0) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            FFmpegGraphError::fromCode(layoutStatus, "av_channel_layout_copy(audio encoder frame)"));
    }
    const int bufferStatus = av_frame_get_buffer(frame.get(), 0);
    if (bufferStatus < 0) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            FFmpegGraphError::fromCode(bufferStatus, "av_frame_get_buffer(audio encoder frame)"));
    }
    const int read = av_audio_fifo_read(m_fifo.get(), reinterpret_cast<void**>(frame->extended_data), samples);
    if (read != samples) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            ::media::ErrorInfo::ffmpegFailure("AudioEncoderFrameQueue failed to dequeue requested samples"));
    }
    frame->pts = m_nextPts;
    frame->pkt_dts = AV_NOPTS_VALUE;
    frame->duration = samples;
    m_nextPts += samples;
    return ::media::Result<::media::ffmpeg::FramePtr>::success(std::move(frame));
}

bool AudioEncoderFrameQueue::configured() const noexcept
{
    return m_fifo && m_sampleFormat != AV_SAMPLE_FMT_NONE &&
           m_sampleRate > 0 && m_frameSize > 0 && m_channelLayout.nb_channels > 0;
}

} // namespace media::ffmpeg::graph
