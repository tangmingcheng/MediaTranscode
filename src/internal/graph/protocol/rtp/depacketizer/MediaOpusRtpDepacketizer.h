#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

namespace media::ffmpeg::graph {

class MediaOpusRtpDepacketizer final : public MediaRtpDepacketizer {
public:
    explicit MediaOpusRtpDepacketizer(MediaRtpDepacketizerConfig config);
    ::media::Result<MediaRtpDepacketizerResult> push(const MediaRtpPacket& packet) override;
    void discontinuity(MediaRtpDiscontinuityReason reason) noexcept override;

private:
    MediaRtpDepacketizerConfig m_config;
};

} // namespace media::ffmpeg::graph
