#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsMaterializedVideoConfig final {
public:
    static ::media::Result<MediaTsMaterializedVideoConfig> create(
        MediaTsH264InputLayout layout,
        std::uint8_t nalLengthBytes,
        std::vector<std::uint8_t> spsAnnexB,
        std::vector<std::uint8_t> ppsAnnexB);

    MediaTsH264InputLayout layout() const noexcept { return m_layout; }
    std::uint8_t nalLengthBytes() const noexcept { return m_nalLengthBytes; }
    const std::vector<std::uint8_t>& spsAnnexB() const noexcept { return m_spsAnnexB; }
    const std::vector<std::uint8_t>& ppsAnnexB() const noexcept { return m_ppsAnnexB; }

private:
    MediaTsMaterializedVideoConfig(
        MediaTsH264InputLayout layout,
        std::uint8_t nalLengthBytes,
        std::vector<std::uint8_t> spsAnnexB,
        std::vector<std::uint8_t> ppsAnnexB) noexcept;

    MediaTsH264InputLayout m_layout;
    std::uint8_t m_nalLengthBytes;
    std::vector<std::uint8_t> m_spsAnnexB;
    std::vector<std::uint8_t> m_ppsAnnexB;
};

struct MediaTsMaterializedAudioConfig final {
    std::uint8_t audioObjectType;
    std::uint8_t samplingFrequencyIndex;
    std::uint8_t channelConfiguration;
};

} // namespace media::ffmpeg::graph
