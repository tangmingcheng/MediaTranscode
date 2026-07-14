#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaOutputByteSinkBuffer final : public MediaBuffer {
public:
    static ::media::Result<std::unique_ptr<MediaOutputByteSinkBuffer>> create(
        std::unique_ptr<MediaOutputByteSink> sink);

    MediaBufferType type() const noexcept override;
    ::media::Result<std::unique_ptr<MediaOutputByteSink>> takeSink();

private:
    explicit MediaOutputByteSinkBuffer(
        std::unique_ptr<MediaOutputByteSink> sink);

    std::unique_ptr<MediaOutputByteSink> m_sink;
};

} // namespace media::ffmpeg::graph
