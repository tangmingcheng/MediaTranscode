#include "internal/graph/nodes/audio/AudioEncoderFrameQueue.h"
#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
}

#include <limits>

namespace media::ffmpeg::graph {

AudioEncoderFrameQueue::AudioEncoderFrameQueue(
    MediaAudioLineageExecutionMode lineageMode,
    std::size_t lineageCapacity)
    : m_lineageMode(lineageMode)
    , m_lineageCapacity(lineageCapacity)
{
}

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

::media::Status AudioEncoderFrameQueue::push(
    const AVFrame& frame,
    std::vector<MediaAudioIntervalFragment> fragments)
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
    const bool timestampOverflow = m_nextPts != AV_NOPTS_VALUE &&
        m_nextPts > std::numeric_limits<std::int64_t>::max() - queued;
    if (timestampOverflow ||
        (m_nextPts != AV_NOPTS_VALUE && frame.pts != m_nextPts + queued) ||
        queued > std::numeric_limits<int>::max() - frame.nb_samples) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioEncoderFrameQueue timestamp or sample count is invalid"));
    }

    const int required = queued + frame.nb_samples;
    auto candidateIntervals = m_intervals;
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        MediaAudioLineageCapacity capacity(m_lineageCapacity);
        if (auto status = m_intervals.observeLineageCapacity(capacity); !status) {
            return status;
        }
        if (auto status = capacity.observe(fragments); !status) {
            return status;
        }
        std::int64_t lineageSamples = 0;
        for (const auto& fragment : fragments) {
            const auto samples = fragment.interval.end - fragment.interval.begin;
            if (samples <= 0 || lineageSamples >
                    std::numeric_limits<std::int64_t>::max() - samples) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Synchronized audio encoder FIFO lineage sample count overflows"));
            }
            lineageSamples += samples;
        }
        if (lineageSamples != frame.nb_samples) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Synchronized audio encoder FIFO requires exact frame lineage"));
        }
        for (auto& fragment : fragments) {
            if (auto status = candidateIntervals.push(std::move(fragment)); !status) {
                return status;
            }
        }
    } else if (!fragments.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Legacy audio encoder FIFO rejects canonical lineage"));
    }
    const int reallocStatus = av_audio_fifo_realloc(m_fifo.get(), required);
    if (reallocStatus < 0) {
        auto status = FFmpegGraphError::statusFromCode(
            reallocStatus, "av_audio_fifo_realloc(audio encoder frame queue)");
        poison();
        return status;
    }

    const int written = av_audio_fifo_write(
        m_fifo.get(), reinterpret_cast<void**>(frame.extended_data), frame.nb_samples);
    if (written != frame.nb_samples) {
        auto status = ::media::Status::failure(
            ::media::ErrorInfo::ffmpegFailure("AudioEncoderFrameQueue failed to enqueue all audio samples"));
        poison();
        return status;
    }
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        m_intervals = std::move(candidateIntervals);
    }
    if (m_nextPts == AV_NOPTS_VALUE) m_nextPts = frame.pts;
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

::media::Status AudioEncoderFrameQueue::observeLineageCapacity(
    MediaAudioLineageCapacity& capacity) const
{
    return m_intervals.observeLineageCapacity(capacity);
}

::media::Result<AudioEncoderFrameQueue::QueuedFrame> AudioEncoderFrameQueue::popFullFrame()
{
    if (!hasFullFrame()) {
        return ::media::Result<QueuedFrame>::failure(
            ::media::ErrorInfo::wouldBlock("AudioEncoderFrameQueue has no complete encoder frame"));
    }
    return popSamples(m_frameSize);
}

::media::Result<AudioEncoderFrameQueue::QueuedFrame> AudioEncoderFrameQueue::popRemainingFrame()
{
    if (queuedSamples() <= 0) {
        return ::media::Result<QueuedFrame>::failure(
            ::media::ErrorInfo::wouldBlock("AudioEncoderFrameQueue has no remaining samples"));
    }
    return popSamples(queuedSamples());
}

void AudioEncoderFrameQueue::clearQueuedSamples() noexcept
{
    if (m_fifo) av_audio_fifo_reset(m_fifo.get());
    m_nextPts = AV_NOPTS_VALUE;
    m_intervals = MediaAudioIntervalAccumulator{};
    m_terminalFailure = false;
}

void AudioEncoderFrameQueue::reset() noexcept
{
    m_fifo.reset();
    av_channel_layout_uninit(&m_channelLayout);
    m_sampleFormat = AV_SAMPLE_FMT_NONE;
    m_sampleRate = 0;
    m_frameSize = 0;
    m_nextPts = AV_NOPTS_VALUE;
    m_intervals = MediaAudioIntervalAccumulator{};
    m_terminalFailure = false;
}

void AudioEncoderFrameQueue::poison() noexcept
{
    m_fifo.reset();
    m_intervals = MediaAudioIntervalAccumulator{};
    m_nextPts = AV_NOPTS_VALUE;
    m_terminalFailure = true;
}

::media::Result<AudioEncoderFrameQueue::QueuedFrame> AudioEncoderFrameQueue::popSamples(int samples)
{
    if (m_nextPts == AV_NOPTS_VALUE || samples <= 0 ||
        m_nextPts > std::numeric_limits<std::int64_t>::max() - samples) {
        return ::media::Result<QueuedFrame>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioEncoderFrameQueue output timestamp overflows"));
    }
    auto candidateIntervals = m_intervals;
    std::vector<MediaAudioIntervalFragment> fragments;
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        auto taken = candidateIntervals.take(samples);
        if (!taken) return ::media::Result<QueuedFrame>::failure(taken.error());
        fragments = std::move(taken).value();
    }
    auto frame = ::media::ffmpeg::makeFrame();
    if (!frame) {
        return ::media::Result<QueuedFrame>::failure(
            ::media::ErrorInfo::allocationFailed("AudioEncoderFrameQueue failed to allocate output frame"));
    }
    frame->format = m_sampleFormat;
    frame->sample_rate = m_sampleRate;
    frame->nb_samples = samples;
    const int layoutStatus = av_channel_layout_copy(&frame->ch_layout, &m_channelLayout);
    if (layoutStatus < 0) {
        return ::media::Result<QueuedFrame>::failure(
            FFmpegGraphError::fromCode(layoutStatus, "av_channel_layout_copy(audio encoder frame)"));
    }
    const int bufferStatus = av_frame_get_buffer(frame.get(), 0);
    if (bufferStatus < 0) {
        return ::media::Result<QueuedFrame>::failure(
            FFmpegGraphError::fromCode(bufferStatus, "av_frame_get_buffer(audio encoder frame)"));
    }
    const int read = av_audio_fifo_read(m_fifo.get(), reinterpret_cast<void**>(frame->extended_data), samples);
    if (read != samples) {
        auto error = ::media::ErrorInfo::ffmpegFailure(
            "AudioEncoderFrameQueue failed to dequeue requested samples");
        poison();
        return ::media::Result<QueuedFrame>::failure(std::move(error));
    }
    frame->pts = m_nextPts;
    frame->pkt_dts = AV_NOPTS_VALUE;
    frame->duration = samples;
    m_nextPts += samples;
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        m_intervals = std::move(candidateIntervals);
    }
    return ::media::Result<QueuedFrame>::success(
        QueuedFrame{std::move(frame), std::move(fragments)});
}

::media::Status AudioEncoderFrameQueue::finishLineage() const
{
    return m_intervals.finish();
}

bool AudioEncoderFrameQueue::configured() const noexcept
{
    return !m_terminalFailure && m_fifo && m_sampleFormat != AV_SAMPLE_FMT_NONE &&
           m_sampleRate > 0 && m_frameSize > 0 && m_channelLayout.nb_channels > 0;
}

} // namespace media::ffmpeg::graph
