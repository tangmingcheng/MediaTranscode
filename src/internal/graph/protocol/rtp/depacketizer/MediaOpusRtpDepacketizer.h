#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

namespace media::ffmpeg::graph {

class MediaOpusRtpDepacketizer final : public MediaRtpDepacketizer {
public:
    explicit MediaOpusRtpDepacketizer(MediaRtpDepacketizerConfig config, MediaLineageAllocationScope allocation);
    ::media::Result<MediaRtpDepacketizerResult> push(const MediaRtpPacket& packet) override;
    void discontinuity(MediaRtpDiscontinuityReason reason) noexcept override;
    MediaRtpAssemblyState assemblyState() const noexcept override { return MediaRtpAssemblyState::Empty; }

private:
    MediaRtpDepacketizerConfig m_config;
};

} // namespace media::ffmpeg::graph
