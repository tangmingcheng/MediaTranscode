#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class AudioEncoderFrameQueue final {
public:
    struct QueuedFrame final {
        ::media::ffmpeg::FramePtr media;
        std::vector<MediaAudioIntervalFragment> fragments;
    };

    AudioEncoderFrameQueue(MediaAudioLineageExecutionMode lineageMode,
                           std::size_t lineageCapacity);
    ~AudioEncoderFrameQueue();

    AudioEncoderFrameQueue(const AudioEncoderFrameQueue&) = delete;
    AudioEncoderFrameQueue& operator=(const AudioEncoderFrameQueue&) = delete;

    ::media::Status configure(const AVCodecContext& codecContext);
    ::media::Status push(
        const AVFrame& frame,
        std::vector<MediaAudioIntervalFragment> fragments);

    bool hasFullFrame() const noexcept;
    bool configured() const noexcept;
    int queuedSamples() const noexcept;
    ::media::Status observeLineageCapacity(
        MediaAudioLineageCapacity& capacity) const;
    ::media::Result<QueuedFrame> popFullFrame();
    ::media::Result<QueuedFrame> popRemainingFrame();
    ::media::Status finishLineage() const;

    void reset() noexcept;

private:
    ::media::Result<QueuedFrame> popSamples(int samples);
    void poison() noexcept;
private:
    ::media::ffmpeg::AudioFifoPtr m_fifo;
    AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE;
    int m_sampleRate = 0;
    int m_frameSize = 0;
    AVChannelLayout m_channelLayout {};
    int64_t m_nextPts = AV_NOPTS_VALUE;
    MediaAudioLineageExecutionMode m_lineageMode;
    std::size_t m_lineageCapacity = 0;
    MediaAudioIntervalAccumulator m_intervals;
    bool m_terminalFailure = false;
};

} // namespace media::ffmpeg::graph
