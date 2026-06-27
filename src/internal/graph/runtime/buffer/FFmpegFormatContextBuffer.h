#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

enum class FFmpegFormatContextOwnership {
    Input,
    Output,
    Borrowed
};

class FFmpegFormatContextBuffer final : public MediaBuffer {
public:
    explicit FFmpegFormatContextBuffer(::media::ffmpeg::InputFormatContextPtr context);
    explicit FFmpegFormatContextBuffer(::media::ffmpeg::OutputFormatContextPtr context);
    explicit FFmpegFormatContextBuffer(AVFormatContext* borrowedContext);

    MediaBufferType type() const noexcept override;

    AVFormatContext* context() noexcept;
    const AVFormatContext* context() const noexcept;

    FFmpegFormatContextOwnership ownership() const noexcept;

    ::media::ffmpeg::InputFormatContextPtr takeInputContext() noexcept;
    ::media::ffmpeg::OutputFormatContextPtr takeOutputContext() noexcept;

private:
    FFmpegFormatContextOwnership m_ownership = FFmpegFormatContextOwnership::Borrowed;
    ::media::ffmpeg::InputFormatContextPtr m_inputContext;
    ::media::ffmpeg::OutputFormatContextPtr m_outputContext;
    AVFormatContext* m_borrowedContext = nullptr;
};

} // namespace media::ffmpeg::graph
