#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/input/FFmpegRealtimeInterruptController.h"
#include "media_transcode/Result.h"

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

enum class RealtimeInputReadState {
    Packet,
    TryAgain,
    EndOfStream,
    Interrupted
};

class FFmpegRealtimeInputSource {
public:
    struct Config {
        std::string inputUrl;
        std::string inputFormatHint;

        int openTimeoutMs = 5000;
        int readTimeoutMs = 5000;
        int analyzeDurationUs = 500000;
        int probeSizeBytes = 512 * 1024;
        bool lowLatency = true;
    };

    FFmpegRealtimeInputSource() = default;
    ~FFmpegRealtimeInputSource();

    FFmpegRealtimeInputSource(const FFmpegRealtimeInputSource&) = delete;
    FFmpegRealtimeInputSource& operator=(const FFmpegRealtimeInputSource&) = delete;
    FFmpegRealtimeInputSource(FFmpegRealtimeInputSource&&) = delete;
    FFmpegRealtimeInputSource& operator=(FFmpegRealtimeInputSource&&) = delete;

    Status open(const Config& config);
    Status findStreamInfo();
    Result<RealtimeInputReadState> readPacket(AVPacket* packet);

    void requestInterrupt();
    void close();

    bool isOpen() const;
    bool isVideoPacket(const AVPacket* packet) const;
    bool isAudioPacket(const AVPacket* packet) const;

    AVFormatContext* formatContext() const;
    AVStream* videoStream() const;
    AVStream* audioStream() const;
    int videoStreamIndex() const;
    int audioStreamIndex() const;

private:
    Status validateConfig(const Config& config) const;
    Status findBestStreams();
    void applyInputOptions(AVDictionary** options) const;
    bool interruptedByRequest() const;

private:
    Config m_config;
    InputFormatContextPtr m_formatContext;
    FFmpegRealtimeInterruptController m_interruptController;

    AVStream* m_videoStream = nullptr;
    AVStream* m_audioStream = nullptr;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
};

} // namespace media::ffmpeg
