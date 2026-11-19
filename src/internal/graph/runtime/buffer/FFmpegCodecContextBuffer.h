#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/model/MediaVideoEncoderCompletionContract.h"
#include <optional>

namespace media::ffmpeg::graph {

enum class FFmpegCodecContextOwnership {
    Owned,
    Borrowed
};

class FFmpegCodecContextBuffer final : public MediaBuffer {
public:
    FFmpegCodecContextBuffer(::media::ffmpeg::CodecContextPtr context,
        std::optional<MediaVideoEncoderCompletionContract> preparedEncoder);
    explicit FFmpegCodecContextBuffer(AVCodecContext* borrowedContext);

    MediaBufferType type() const noexcept override;

    AVCodecContext* context() noexcept;
    const AVCodecContext* context() const noexcept;

    MediaVideoDecoderCallbackState* decoderCallbackState() const noexcept
    { return m_context ? m_context.get_deleter().callbackOwner.get() : nullptr; }
    FFmpegCodecContextOwnership ownership() const noexcept;
    ::media::ffmpeg::CodecContextPtr takeContext() noexcept;
    const std::optional<MediaVideoEncoderCompletionContract>& preparedEncoder() const noexcept
    { return m_preparedEncoder; }

private:
    FFmpegCodecContextOwnership m_ownership = FFmpegCodecContextOwnership::Borrowed;
    ::media::ffmpeg::CodecContextPtr m_context;
    AVCodecContext* m_borrowedContext = nullptr;
    const std::optional<MediaVideoEncoderCompletionContract> m_preparedEncoder;
};

} // namespace media::ffmpeg::graph
