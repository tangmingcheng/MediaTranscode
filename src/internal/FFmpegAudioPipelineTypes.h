#pragma once

#include "internal/TranscodeTypes.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>

extern "C" {
typedef struct AVFrame AVFrame;
typedef struct AVPacket AVPacket;
typedef struct AVStream AVStream;
}

namespace media::ffmpeg {

class AudioOutputStreamProvider;
class PacketOutputNode;
class TimelineNormalizer;

using FFmpegAudioPacketWrittenCallback =
    std::function<void(int64_t packetCount, int64_t outTimeMs)>;

enum class FFmpegAudioPipelineMode {
    None,
    Copy,
    Encode,

    // Internal compatibility aliases only. Do not use for public API.
    CopySelected = Copy,
    EncodeSelected = Encode
};

using AudioMode = FFmpegAudioPipelineMode;

struct FFmpegAudioPipelineConfig {
    /*
     * inputAudioStream and timeline are borrowed from the local file transcode
     * engine. outputStreamProvider creates the muxer-specific audio stream;
     * outputNode receives normalized/encoded packets through the output graph.
     */
    FFmpegAudioPipelineMode mode = FFmpegAudioPipelineMode::None;
    AudioCodec codec = AudioCodec::AAC;
    AVStream* inputAudioStream = nullptr;
    AudioOutputStreamProvider* outputStreamProvider = nullptr;
    PacketOutputNode* outputNode = nullptr;
    TimelineNormalizer* timeline = nullptr;
    int audioBitrateKbps = 128;
};

struct FFmpegAudioPacketProgress {
    int64_t packetCount = 0;
    int64_t lastWrittenOutTimeMs = 0;
};

const char* audioPipelineModeName(FFmpegAudioPipelineMode mode);

} // namespace media::ffmpeg
