#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaHevcRtpDepacketizer final : public MediaRtpDepacketizer {
public:
    explicit MediaHevcRtpDepacketizer(MediaRtpDepacketizerConfig config);
    ::media::Result<MediaRtpDepacketizerResult> push(const MediaRtpPacket& packet) override;
    void discontinuity(MediaRtpDiscontinuityReason reason) noexcept override;

private:
    ::media::Result<MediaRtpDepacketizerResult> pushValidated(const MediaRtpPacket& packet);
    ::media::Result<MediaRtpDepacketizerResult> finish(const MediaRtpPacket& packet);
    MediaRtpDepacketizerConfig m_config;
    std::vector<uint8_t> m_accessUnit;
    std::optional<uint32_t> m_timestamp;
    std::optional<uint16_t> m_fragmentNalHeader;
    bool m_fragmentOpen = false;
    bool m_keyFrame = false;
};

} // namespace media::ffmpeg::graph
