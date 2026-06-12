#include "internal/FFmpegAudioFifo.h"

#include "internal/FFmpegUtils.h"

#include <utility>

namespace media::ffmpeg {

FFmpegAudioFifo::~FFmpegAudioFifo()
{
    reset();
}

FFmpegAudioFifo::FFmpegAudioFifo(FFmpegAudioFifo&& other) noexcept
{
    *this = std::move(other);
}

FFmpegAudioFifo& FFmpegAudioFifo::operator=(FFmpegAudioFifo&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_fifo = other.m_fifo;
    m_sampleFormat = other.m_sampleFormat;
    m_channels = other.m_channels;

    other.m_fifo = nullptr;
    other.m_sampleFormat = AV_SAMPLE_FMT_NONE;
    other.m_channels = 0;

    return *this;
}

void FFmpegAudioFifo::reset()
{
    if (m_fifo) {
        av_audio_fifo_free(m_fifo);
    }

    m_fifo = nullptr;
    m_sampleFormat = AV_SAMPLE_FMT_NONE;
    m_channels = 0;
}

bool FFmpegAudioFifo::initialize(AVSampleFormat sampleFormat,
                                 int channels,
                                 int initialSamples,
                                 std::string* error)
{
    reset();

    if (sampleFormat == AV_SAMPLE_FMT_NONE) {
        if (error) {
            *error = "FFmpegAudioFifo initialize failed: invalid sample format";
        }
        return false;
    }

    if (channels <= 0) {
        if (error) {
            *error = "FFmpegAudioFifo initialize failed: invalid channel count";
        }
        return false;
    }

    if (initialSamples <= 0) {
        initialSamples = 1024;
    }

    m_fifo = av_audio_fifo_alloc(sampleFormat, channels, initialSamples);
    if (!m_fifo) {
        if (error) {
            *error = "av_audio_fifo_alloc failed";
        }
        return false;
    }

    m_sampleFormat = sampleFormat;
    m_channels = channels;
    return true;
}

bool FFmpegAudioFifo::isInitialized() const
{
    return m_fifo != nullptr;
}

int FFmpegAudioFifo::size() const
{
    return m_fifo ? av_audio_fifo_size(m_fifo) : 0;
}

int FFmpegAudioFifo::space() const
{
    return m_fifo ? av_audio_fifo_space(m_fifo) : 0;
}

bool FFmpegAudioFifo::ensureAdditionalCapacity(int additionalSamples, std::string* error)
{
    if (!m_fifo) {
        if (error) {
            *error = "FFmpegAudioFifo ensureAdditionalCapacity failed: fifo is not initialized";
        }
        return false;
    }

    if (additionalSamples <= 0) {
        return true;
    }

    const int targetSize = av_audio_fifo_size(m_fifo) + additionalSamples;
    const int ret = av_audio_fifo_realloc(m_fifo, targetSize);
    if (ret < 0) {
        if (error) {
            *error = "av_audio_fifo_realloc failed: " + errorString(ret);
        }
        return false;
    }

    return true;
}

bool FFmpegAudioFifo::writeFrame(AVFrame* frame, std::string* error)
{
    if (!m_fifo) {
        if (error) {
            *error = "FFmpegAudioFifo writeFrame failed: fifo is not initialized";
        }
        return false;
    }

    if (!frame || frame->nb_samples <= 0) {
        return true;
    }

    if (!ensureAdditionalCapacity(frame->nb_samples, error)) {
        return false;
    }

    const int writtenSamples = av_audio_fifo_write(
        m_fifo,
        reinterpret_cast<void**>(frame->extended_data),
        frame->nb_samples
    );

    if (writtenSamples < frame->nb_samples) {
        if (error) {
            *error = "av_audio_fifo_write failed: short write";
        }
        return false;
    }

    return true;
}

bool FFmpegAudioFifo::readToFrame(AVFrame* frame, int samples, std::string* error)
{
    if (!m_fifo) {
        if (error) {
            *error = "FFmpegAudioFifo readToFrame failed: fifo is not initialized";
        }
        return false;
    }

    if (!frame) {
        if (error) {
            *error = "FFmpegAudioFifo readToFrame failed: frame is null";
        }
        return false;
    }

    if (samples <= 0) {
        return true;
    }

    if (av_audio_fifo_size(m_fifo) < samples) {
        if (error) {
            *error = "FFmpegAudioFifo readToFrame failed: insufficient samples";
        }
        return false;
    }

    const int readSamples = av_audio_fifo_read(
        m_fifo,
        reinterpret_cast<void**>(frame->extended_data),
        samples
    );

    if (readSamples < samples) {
        if (error) {
            *error = "av_audio_fifo_read failed: short read";
        }
        return false;
    }

    return true;
}

AVAudioFifo* FFmpegAudioFifo::raw() const
{
    return m_fifo;
}

} // namespace media::ffmpeg
