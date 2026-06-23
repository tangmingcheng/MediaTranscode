#pragma once

#include "media_transcode/MediaTranscodeTypes.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>

extern "C" {
typedef struct AVFormatContext AVFormatContext;
typedef struct AVFrame AVFrame;
typedef struct AVPacket AVPacket;
typedef struct AVStream AVStream;
}

namespace media::ffmpeg {

class TimelineNormalizer;

using FFmpegAudioPacketWrittenCallback =
    std::function<void(int64_t packetCount, int64_t outTimeMs)>;

enum class FFmpegAudioPipelineMode {
    None,
    Copy,
    Encode
};

struct FFmpegAudioPipelineConfig {
    /*
     * inputAudioStream, outputFmtCtx and timeline are borrowed from FFmpegTranscoder.
     * Concrete pipeline strategies own their decoder / encoder / resampler / fifo resources.
     */
    FFmpegAudioPipelineMode mode = FFmpegAudioPipelineMode::None;
    AudioCodec codec = AudioCodec::AAC;
    AVStream* inputAudioStream = nullptr;
    AVFormatContext* outputFmtCtx = nullptr;
    TimelineNormalizer* timeline = nullptr;
    int audioBitrateKbps = 128;
};

struct FFmpegAudioPacketProgress {
    int64_t packetCount = 0;
    int64_t lastWrittenOutTimeMs = 0;
};

const char* audioPipelineModeName(FFmpegAudioPipelineMode mode);

} // namespace media::ffmpeg
