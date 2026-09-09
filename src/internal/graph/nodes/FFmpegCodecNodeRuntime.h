#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"

namespace media::ffmpeg::graph {

class FFmpegCodecNodeRuntime : public FFmpegNodeRuntime {
public:
    FFmpegCodecNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name);
    ~FFmpegCodecNodeRuntime() override = default;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    bool tryBindCodecContext(const MediaBufferRef& buffer) noexcept;
    AVCodecContext* codecContext() noexcept;
    const AVCodecContext* codecContext() const noexcept;
    bool hasCodecContext() const noexcept;
    bool codecMetadataPublished() const noexcept;
    ::media::Status publishCodecMetadata(MediaGraphExecutionContext& context);

private:
    void resetCodecContext() noexcept;
    MediaBufferRef m_codecContextOwner;
    AVCodecContext* m_codecContext = nullptr;
    MediaBufferRef m_codecParametersSnapshot;
    bool m_contextMetadataPublished = false;
    bool m_parametersMetadataPublished = false;
};

} // namespace media::ffmpeg::graph
