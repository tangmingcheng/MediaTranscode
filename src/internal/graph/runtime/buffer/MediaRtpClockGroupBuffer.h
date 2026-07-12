#pragma once

#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

class MediaRtpClockGroupBuffer final : public MediaBuffer {
public:
    explicit MediaRtpClockGroupBuffer(MediaRtpClockGroupSnapshot snapshot);

    MediaBufferType type() const noexcept override;
    const MediaRtpClockGroupSnapshot& snapshot() const noexcept;

private:
    const MediaRtpClockGroupSnapshot m_snapshot;
};

} // namespace media::ffmpeg::graph
