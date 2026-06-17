#pragma once

#include "internal/FFmpegRAII.h"

#include <string>

extern "C" {
#include <libavutil/audio_fifo.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

namespace media::ffmpeg {

class FFmpegAudioFifo {
public:
    FFmpegAudioFifo() = default;
    ~FFmpegAudioFifo();

    FFmpegAudioFifo(const FFmpegAudioFifo&) = delete;
    FFmpegAudioFifo& operator=(const FFmpegAudioFifo&) = delete;

    FFmpegAudioFifo(FFmpegAudioFifo&& other) noexcept;
    FFmpegAudioFifo& operator=(FFmpegAudioFifo&& other) noexcept;

    void reset();

    bool initialize(AVSampleFormat sampleFormat,
                    int channels,
                    int initialSamples,
                    std::string* error);

    bool isInitialized() const;
    int size() const;
    int space() const;

    bool ensureAdditionalCapacity(int additionalSamples, std::string* error);
    bool writeFrame(AVFrame* frame, std::string* error);
    bool readToFrame(AVFrame* frame, int samples, std::string* error);

    AVAudioFifo* raw() const;

private:
    AudioFifoPtr m_fifo;
    AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE;
    int m_channels = 0;
};

} // namespace media::ffmpeg
