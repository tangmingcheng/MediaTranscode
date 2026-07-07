#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

enum class FFmpegCodecContextOwnership {
    Owned,
    Borrowed
};

class FFmpegCodecContextBuffer final : public MediaBuffer {
public:
    explicit FFmpegCodecContextBuffer(::media::ffmpeg::CodecContextPtr context);
    explicit FFmpegCodecContextBuffer(AVCodecContext* borrowedContext);

    MediaBufferType type() const noexcept override;

    AVCodecContext* context() noexcept;
    const AVCodecContext* context() const noexcept;

    FFmpegCodecContextOwnership ownership() const noexcept;
    ::media::ffmpeg::CodecContextPtr takeContext() noexcept;

private:
    FFmpegCodecContextOwnership m_ownership = FFmpegCodecContextOwnership::Borrowed;
    ::media::ffmpeg::CodecContextPtr m_context;
    AVCodecContext* m_borrowedContext = nullptr;
};

} // namespace media::ffmpeg::graph
