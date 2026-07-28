#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaAacRtpDepacketizer final : public MediaRtpDepacketizer {
public:
    explicit MediaAacRtpDepacketizer(MediaRtpDepacketizerConfig config);
    ::media::Result<MediaRtpDepacketizerResult> push(const MediaRtpPacket& packet) override;
    void discontinuity(MediaRtpDiscontinuityReason reason) noexcept override;

private:
    struct FragmentedAccessUnit final {
        std::size_t expectedSize;
        std::uint64_t index;
        std::uint32_t timestamp;
        std::uint16_t lastSequenceNumber;
        std::vector<std::uint8_t> bytes;
    };

    MediaRtpDepacketizerConfig m_config;
    std::optional<FragmentedAccessUnit> m_fragmentedAccessUnit;
};

} // namespace media::ffmpeg::graph
