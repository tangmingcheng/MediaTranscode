#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/output/AudioOutputStreamProvider.h"
#include "internal/output/FFmpegMuxerOutputNode.h"
#include "internal/output/PacketOutputGraphController.h"
#include "internal/output/PacketOutputNode.h"
#include "internal/output/VideoOutputStreamProvider.h"
#include "media_transcode/Result.h"

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegFileOutputSession final {
public:
    struct Config {
        std::string outputUrl;
    };

    FFmpegFileOutputSession() = default;
    ~FFmpegFileOutputSession();

    FFmpegFileOutputSession(const FFmpegFileOutputSession&) = delete;
    FFmpegFileOutputSession& operator=(const FFmpegFileOutputSession&) = delete;
    FFmpegFileOutputSession(FFmpegFileOutputSession&&) = delete;
    FFmpegFileOutputSession& operator=(FFmpegFileOutputSession&&) = delete;

    void reset();

    Status initialize(Config config);
    Status openIo();
    Status writeHeader();
    Status writeTrailer();

    bool isInitialized() const;
    bool headerWritten() const;

    AVFormatContext* context() const;
    VideoOutputStreamProvider* videoStreamProvider();
    AudioOutputStreamProvider* audioStreamProvider();
    PacketOutputNode* outputNode();

private:
    std::string m_outputUrl;
    OutputFormatContextPtr m_outputFmtCtx;
    PacketOutputGraphController m_outputGraphController;
    FFmpegMuxerOutputNode m_muxerOutputNode;
    bool m_headerWritten = false;
};

} // namespace media::ffmpeg
