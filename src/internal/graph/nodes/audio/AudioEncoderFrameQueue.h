#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class AudioEncoderFrameQueue final {
public:
    AudioEncoderFrameQueue() = default;
    ~AudioEncoderFrameQueue();

    AudioEncoderFrameQueue(const AudioEncoderFrameQueue&) = delete;
    AudioEncoderFrameQueue& operator=(const AudioEncoderFrameQueue&) = delete;

    ::media::Status configure(const AVCodecContext& codecContext);
    ::media::Status push(const AVFrame& frame);

    bool hasFullFrame() const noexcept;
    bool configured() const noexcept;
    int queuedSamples() const noexcept;
    ::media::Result<::media::ffmpeg::FramePtr> popFullFrame();
    ::media::Result<::media::ffmpeg::FramePtr> popRemainingFrame();

    void reset() noexcept;

private:
    ::media::Result<::media::ffmpeg::FramePtr> popSamples(int samples);
private:
    ::media::ffmpeg::AudioFifoPtr m_fifo;
    AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE;
    int m_sampleRate = 0;
    int m_frameSize = 0;
    AVChannelLayout m_channelLayout {};
    int64_t m_nextPts = AV_NOPTS_VALUE;
};

} // namespace media::ffmpeg::graph
