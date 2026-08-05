#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "media_transcode/Result.h"

#include <deque>
#include <memory>

namespace media::ffmpeg::graph {

struct MediaPreparedRawRtpInput final {
    MediaRtpUdpTransport transport;
    std::deque<MediaRtpUdpDatagram> datagrams;
    MediaDetectedRtpVideoSignaling signaling;
};

class MediaRawRtpPreparedInputBuffer final : public MediaBuffer {
public:
    static ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>
    create(MediaPreparedRawRtpInput prepared);

    MediaBufferType type() const noexcept override;
    ::media::Result<MediaPreparedRawRtpInput> takePreparedInput();

private:
    explicit MediaRawRtpPreparedInputBuffer(MediaPreparedRawRtpInput prepared);

    std::optional<MediaPreparedRawRtpInput> m_prepared;
};

} // namespace media::ffmpeg::graph
