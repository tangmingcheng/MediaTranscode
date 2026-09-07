#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/MediaRtpAccessUnitContinuity.h"
#include "internal/graph/protocol/rtp/MediaRtpNalUnitParser.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaH264RtpDepacketizer final : public MediaRtpDepacketizer {
public:
    MediaH264RtpDepacketizer(MediaRtpDepacketizerConfig config,
                               std::size_t maximumAccessUnitBytes);
    ::media::Result<MediaRtpDepacketizerResult> push(const MediaRtpPacket& packet) override;
    void discontinuity(MediaRtpDiscontinuityReason reason) noexcept override;

private:
    ::media::Result<MediaRtpDepacketizerResult> pushValidated(const MediaRtpPacket& packet);
    ::media::Result<MediaRtpDepacketizerResult> finish(const MediaRtpPacket& packet);
    MediaRtpDepacketizerConfig m_config;
    std::size_t m_maximumAccessUnitBytes;
    MediaRtpAccessUnitContinuity m_continuity;
    MediaH264RtpNalUnitParser m_nalParser;
    std::vector<uint8_t> m_accessUnit;
    std::optional<uint32_t> m_timestamp;
    bool m_keyFrame = false;
};

} // namespace media::ffmpeg::graph
