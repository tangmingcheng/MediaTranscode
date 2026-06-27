#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"

namespace media::ffmpeg::graph {

class FFmpegCodecNodeRuntime : public FFmpegNodeRuntime {
public:
    FFmpegCodecNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name);
    ~FFmpegCodecNodeRuntime() override = default;

protected:
    bool tryBindCodecContext(const MediaBufferRef& buffer) noexcept;
    AVCodecContext* codecContext() noexcept;
    const AVCodecContext* codecContext() const noexcept;
    bool hasCodecContext() const noexcept;

private:
    MediaBufferRef m_codecContextOwner;
    AVCodecContext* m_codecContext = nullptr;
};

} // namespace media::ffmpeg::graph
