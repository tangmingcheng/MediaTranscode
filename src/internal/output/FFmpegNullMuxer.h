#pragma once

#include "internal/FFmpegRAII.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

/**
 * @brief Minimal FFmpeg null muxer wrapper for P1 realtime encode probing.
 *
 * The null muxer lets the existing video transcode pipeline create an output
 * stream and write encoded packets without producing a file or network output.
 */
class FFmpegNullMuxer {
public:
    FFmpegNullMuxer() = default;
    ~FFmpegNullMuxer();

    FFmpegNullMuxer(const FFmpegNullMuxer&) = delete;
    FFmpegNullMuxer& operator=(const FFmpegNullMuxer&) = delete;
    FFmpegNullMuxer(FFmpegNullMuxer&&) = delete;
    FFmpegNullMuxer& operator=(FFmpegNullMuxer&&) = delete;

    Status open();
    Status writeHeader();
    Status writeTrailer();
    void reset();

    AVFormatContext* context() const;
    bool isOpen() const;
    bool headerWritten() const;

private:
    OutputFormatContextPtr m_context;
    bool m_headerWritten = false;
};

} // namespace media::ffmpeg
