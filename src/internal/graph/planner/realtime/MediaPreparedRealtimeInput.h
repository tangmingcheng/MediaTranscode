#pragma once

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <memory>
#include <functional>
#include <string>

namespace media::ffmpeg::graph {

using MediaRealtimeInputOpener = std::function<::media::Result<::media::ffmpeg::InputFormatContextPtr>(
    const std::string&, AVDictionary**)>;

class MediaPreparedRealtimeInput final {
public:
    MediaPreparedRealtimeInput() = default;
    ~MediaPreparedRealtimeInput() = default;

    MediaPreparedRealtimeInput(const MediaPreparedRealtimeInput&) = delete;
    MediaPreparedRealtimeInput& operator=(const MediaPreparedRealtimeInput&) = delete;
    MediaPreparedRealtimeInput(MediaPreparedRealtimeInput&&) noexcept = default;
    MediaPreparedRealtimeInput& operator=(MediaPreparedRealtimeInput&&) noexcept = default;

    static ::media::Result<MediaPreparedRealtimeInput> create(
        ::media::ffmpeg::InputFormatContextPtr context);

    bool valid() const noexcept;
    AVFormatContext* context() noexcept;
    const AVFormatContext* context() const noexcept;
    const FFmpegInputStreamSnapshot* inputStreamSnapshot(int streamIndex) const noexcept;
    MediaBufferRef releaseFormatBuffer() noexcept;

private:
    explicit MediaPreparedRealtimeInput(std::unique_ptr<FFmpegFormatContextBuffer> buffer);

    std::unique_ptr<FFmpegFormatContextBuffer> m_buffer;
};

} // namespace media::ffmpeg::graph
