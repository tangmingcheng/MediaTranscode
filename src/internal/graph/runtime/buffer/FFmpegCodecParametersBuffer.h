#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

struct AVCodecParameters;

namespace media::ffmpeg::graph {

class FFmpegCodecParametersBuffer final : public MediaBuffer {
public:
    explicit FFmpegCodecParametersBuffer(::media::ffmpeg::CodecParametersPtr parameters);

    MediaBufferType type() const noexcept override;

    AVCodecParameters* parameters() noexcept;
    const AVCodecParameters* parameters() const noexcept;

private:
    ::media::ffmpeg::CodecParametersPtr m_parameters;
};

} // namespace media::ffmpeg::graph
