#pragma once

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <memory>
#include <functional>
#include <string>

namespace media::ffmpeg::graph {

using MediaRealtimeInputOpener = std::function<::media::Result<::media::ffmpeg::InputFormatContextPtr>(
    const std::string&, AVDictionary**)>;
using MediaTsInputSessionOpener = std::function<
    ::media::Result<std::unique_ptr<MediaTsInputSession>>(
        const MediaTsInputSessionOptions&)>;

struct MediaRealtimePreflightIo final {
    MediaRealtimeInputOpener openGeneric;
    MediaTsInputSessionOpener openMpegTs;
};

enum class MediaPreparedRealtimeInputKind { Generic, MpegTs };

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
    static ::media::Result<MediaPreparedRealtimeInput> createMpegTs(
        std::unique_ptr<MediaTsInputSession> session);

    bool valid() const noexcept;
    std::optional<MediaPreparedRealtimeInputKind> kind() const noexcept;
    const FFmpegInputStreamSnapshot* inputStreamSnapshot(int streamIndex) const noexcept;
    ::media::Result<MediaBufferRef> releaseBuffer() noexcept;

private:
    explicit MediaPreparedRealtimeInput(std::unique_ptr<FFmpegFormatContextBuffer> buffer);
    explicit MediaPreparedRealtimeInput(std::unique_ptr<MediaTsPreparedInputBuffer> buffer);

    std::unique_ptr<FFmpegFormatContextBuffer> m_genericBuffer;
    std::unique_ptr<MediaTsPreparedInputBuffer> m_tsBuffer;
};

} // namespace media::ffmpeg::graph
