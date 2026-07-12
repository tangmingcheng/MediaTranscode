#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

namespace media::ffmpeg::graph {

class MediaAacRtpDepacketizer final : public MediaRtpDepacketizer {
public:
    explicit MediaAacRtpDepacketizer(MediaRtpDepacketizerConfig config);
    ::media::Result<MediaRtpDepacketizerResult> push(const MediaRtpPacket& packet) override;
    void discontinuity(MediaRtpDiscontinuityReason reason) noexcept override;

private:
    MediaRtpDepacketizerConfig m_config;
};

} // namespace media::ffmpeg::graph
